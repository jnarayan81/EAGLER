#include <seqan/bam_io.h>
#include <seqan/sequence.h>
#include <vector>
#include <algorithm>
#include <string>
#include <stdexcept>

#include "./connector.h"
#include "./utility.h"
#include "./bwa.h"

using std::runtime_error;
using std::vector;
using std::max;
using std::min;
using std::string;
using seqan::BamHeader;
using seqan::BamAlignmentRecord;
using seqan::length;

const char* Connector::reference_file = "./tmp/connector.fasta";
const char* Connector::anchors_file = "./tmp/anchors.fasta";


Connector::Connector(const vector<Contig*>& contigs):
                    contigs_(contigs) {
    for (auto contig : contigs_) {
        string id = utility::CharString_to_string(contig->id());
        unused_contigs[id] = contig;
    }
}


Connector::~Connector() {
    curr = nullptr;

    for (auto scaffold : scaffolds) {
        delete scaffold;
    }
}


void Connector::connect_contigs() {
    Contig::dump_anchors(contigs_);


    curr = create_scaffold();
    scaffolds.emplace_back(curr);

    bool found = false;
    while (true) {
        found = connect_next();
        std::cout << "After conn next, found: " << found << std::endl;

        if (!found) {
            curr = create_scaffold();
            if (curr != nullptr) {
                scaffolds.emplace_back(curr);
            } else {
                break;
            }
        }
    }

    for (auto scaffold : scaffolds) {
        scaffold->circular_genome_trim();
    }
}


bool Connector::connect_next() {
    Contig *curr_contig = curr->last_contig();
    string curr_contig_id = utility::CharString_to_string(curr_contig->id());

    std::cout << "Current contig: " << curr_contig->id() << std::endl;

    utility::write_fasta(curr_contig->id(), curr_contig->seq(), reference_file);
    aligner::bwa_index(reference_file);
    aligner::bwa_mem(reference_file, anchors_file);

    BamHeader header;
    vector<BamAlignmentRecord> records;
    utility::read_sam(&header, &records, aligner::tmp_alignment_filename);

    for (auto const& record : records) {
        std::cout << "Examining record for anchor: " << record.qName
            << std::endl;

        if ((record.flag & UNMAPPED) || (record.flag & SECONDARY_LINE)) {
            continue;
        }

        if (used_ids_.count(utility::CharString_to_string(record.qName)) > 0) {
            continue;
        }

        string anchor_id = utility::CharString_to_string(record.qName);
        string next_id = anchor_id.substr(0, anchor_id.length() - 1);

        if (next_id == curr_contig_id) {
            std::cout << "Id's are same:" << std::endl
            << next_id << std::endl
            << curr_contig_id << std::endl;
            continue;
        }

        // ovo je mozda problematicno - provjeriti!
        if (!should_connect(curr_contig, record)) {
            continue;
        }

        std::cout << "Attempting merge for anchor: " << record.qName
            << std::endl;

        Contig *next = find_contig(next_id);
        int merge_start = max(curr_contig->right_ext_pos(), record.beginPos);

        if (curr->contains(next_id)) {
            break;
        }

        bool reverse_complement = record.flag & COMPLEMENT;
        if (reverse_complement) next->reverse_complement();

        if (next == nullptr) {
            utility::throw_exception<runtime_error>("Contig invalid id");
        }

        int right_ext_len = curr_contig->total_len() - merge_start;
        int next_start = min(right_ext_len, next->total_ext_left());
        int merge_end = next_start + record.beginPos;

        int merge_len = merge_end - merge_start;

        curr->add_contig(next, merge_start + merge_len / 2,
                         next_start - merge_len / 2);


        used_ids_.insert(anchor_id);
        used_ids_.insert(utility::CharString_to_string(
            curr_contig->right_id()));

        unused_contigs.erase(next_id);

        return true;
    }

    return false;
}


bool Connector::should_connect(Contig *contig,
                               const BamAlignmentRecord& record) {
    // iterate over cigar string to get lengths of
    // read and contig parts used in alignment
    int cigar_len = length(record.cigar);
    int used_read_size = 0;
    int used_contig_size = 0;

    // if the anchor is not soft clipped skip it
    if (record.cigar[cigar_len - 1].operation != 'S') {
        return false;
    }

    for (auto const& e : record.cigar) {
        if (utility::contributes_to_seq_len(e.operation)) {
            used_read_size += e.count;
        }
        if (utility::contributes_to_contig_len(e.operation)) {
            used_contig_size += e.count;
        }
    }

    int right_clipping_len = record.cigar[cigar_len - 1].count;
    used_read_size -= right_clipping_len;
    int len = right_clipping_len -
              (contig->total_len() - (record.beginPos + used_contig_size));

    // if read doesn't extend right of contig skip it
    return len > ANCHOR_THRESHOLD * ANCHOR_LEN;
}


Contig* Connector::find_contig(const string& id) {
    CharString contig_id = id;

    for (auto contig : contigs_) {
        if (contig->id() == contig_id) {
            return contig;
        }
    }

    return nullptr;
}


Scaffold* Connector::create_scaffold() {
    std::cout << "Creating scaffold" << std::endl;
    if (unused_contigs.empty()) {
        return nullptr;
    }
    std::cout << "Scaffold created" << std::endl;
    auto it = unused_contigs.begin();
    Scaffold *scaffold = new Scaffold(it->second);
    unused_contigs.erase(it);
    return scaffold;
}


void Connector::dump_scaffolds() {
    StringSet<Dna5String> scaffold_seqs;
    StringSet<CharString> ids;

    int i = 0;
    for (auto scaffold : scaffolds) {
        appendValue(ids, utility::create_seq_id("scaffold|%d", i));
        appendValue(scaffold_seqs, scaffold->get_combined_sequence());
        ++i;
    }

    utility::write_fasta(ids, scaffold_seqs, "./tmp/genome.fasta");
}
