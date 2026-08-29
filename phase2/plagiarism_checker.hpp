#include "structures.hpp"
// -----------------------------------------------------------------------------
#include <vector>
#include <deque>
#include <queue>
#include <utility>
#include <tuple>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <algorithm>
#include <string>
#include <condition_variable>
#include <atomic>
// You are free to add any STL includes above this comment, below the --line--.
// DO NOT add "using namespace std;" or include any other files/libraries.
// Also DO NOT add the include "bits/stdc++.h"

// OPTIONAL: Add your helper functions and classes here

class plagiarism_checker_t {
    // You should NOT modify the public interface of this class.
public:
    plagiarism_checker_t(void);
    plagiarism_checker_t(std::vector<std::shared_ptr<submission_t>> 
                            __submissions);
    ~plagiarism_checker_t(void);
    void add_submission(std::shared_ptr<submission_t> __submission);

protected:
    // TODO: Add members and function signatures here
    
    // 2 queues for submission and tokenization 
    std::deque<std::shared_ptr<submission_t>> tokenize_queue;
    std::deque<std::shared_ptr<submission_t>> submission_queue;
    
    // mutex and condition variables for the queues
    std::mutex tokenize_queue_mutex;
    std::mutex submission_queue_mutex;
    std::condition_variable tokenize_queue_cv;
    std::condition_variable submission_queue_cv;

    // threads for tokenization and processing
    std::thread tokenize_thread;
    std::thread submission_thread;

    // thread control
    std::atomic<bool> is_initialized{false};
    std::atomic<bool> should_terminate{false};
    
    void process_submission();
    void process_tokenization();

    std::atomic<bool> tokenize_queue_empty{false};
    std::atomic<bool> submission_queue_empty{false};

    // for storing timestamp of each submission
    std::unordered_map< std::shared_ptr<submission_t>, std::chrono::time_point<std::chrono::steady_clock> > map_timestamp;
    std::shared_mutex map_timestamp_mutex;
    void insert_timestamp(std::shared_ptr<submission_t>& submission, std::chrono::time_point<std::chrono::steady_clock> timestamp);
    bool get_timestamp( std::shared_ptr<submission_t>& key, std::chrono::time_point<std::chrono::steady_clock>& value);
    


    // for storing tokens
    std::unordered_map< std::shared_ptr<submission_t>, std::vector<int> > map_tokens;
    std::shared_mutex map_tokens_mutex;
    void insert_tokens(std::shared_ptr<submission_t>& submission, std::vector<int> tokens);
    bool get_tokens(std::shared_ptr<submission_t>& key, std::vector<int>& value);

    // map for hash values,submission pointer and starting index
    std::unordered_map<size_t, std::vector<std::tuple<std::shared_ptr<submission_t>, int>>> hash_map;
    std::shared_mutex hash_map_mutex;
    void insert_hash(size_t hash, std::shared_ptr<submission_t>& submission, int index);
    bool get_hash(size_t key, std::vector<std::tuple<std::shared_ptr<submission_t>, int>>& values);


    void compute_rolling_hash(std::vector<int> &tokens, std::shared_ptr<submission_t> submission, int length=15);
    bool check_for_plag_1_2(std::shared_ptr<submission_t> submission);
    bool search_for_hash1(size_t hash, std::shared_ptr<submission_t> submission, std::unordered_map<size_t, std::set<std::tuple<std::shared_ptr<submission_t>, int>>>& matched_locations, std::unordered_map<std::shared_ptr<submission_t>, int>& match_count);
    bool search_for_hash2(size_t hash, std::shared_ptr<submission_t> submission, std::unordered_map<size_t, std::set<std::tuple<std::shared_ptr<submission_t>, int>>>& matched_locations2, int& counter);
    void check_for_plag_3(std::shared_ptr<submission_t> submission);
    bool search_for_hash_3(size_t hash, std::shared_ptr<submission_t> submission, int index);

    // End TODO
};
