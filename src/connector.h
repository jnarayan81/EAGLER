#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <seqan/bam_io.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include "./contig.h"
#include "./scaffold.h"

using std::vector;
using std::string;
using std::unordered_set;
using std::unordered_map;
using seqan::BamAlignmentRecord;


/**
 * @brief SAM file flag denoting a non-primary alignment
 */
#define SECONDARY_LINE 0x900

/**
 * @brief SAM file flag denoting an unmapped query
 */
#define UNMAPPED 0x4

/**
 * @brief [brief description] SAM file flag denoting a query from the complement
 * strand of the reference genome
 */
#define COMPLEMENT 0x10

/**
 * @brief Minimum percentage of anchor sequence that must extend the end of the
 * current scaffold to extend it
 */
#define ANCHOR_THRESHOLD 0.66


class Connector {
 public:
    explicit Connector(const vector<Contig*>& contigs);
    ~Connector();
    void connect_contigs();
    const vector<Scaffold*>& get_scaffolds() { return scaffolds; }
    void dump_scaffolds();

 private:
    static const char* reference_file;
    static const char* anchors_file;

    const vector<Contig*>& contigs_;
    
    unordered_set<string> used_ids_;
    unordered_map<string, Contig*> unused_contigs;


    Scaffold* curr;
    vector<Scaffold*> scaffolds;
    unordered_map<string, Scaffold*> contig_to_scaffold;

    Scaffold* create_scaffold();
    Contig* find_contig(const string& id);
    bool connect_next();
    bool should_connect(Contig *contig, const BamAlignmentRecord& record);
};


#endif  // CONNECTOR_H
