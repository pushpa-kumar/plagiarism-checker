#include "plagiarism_checker.hpp"
// You should NOT add ANY other includes to this file.
// Do NOT add "using namespace std;".

// TODO: Implement the methods of the plagiarism_checker_t class



// for storing time stamps of each submission
void plagiarism_checker_t::insert_timestamp(std::shared_ptr<submission_t>& submission, std::chrono::time_point<std::chrono::steady_clock> timestamp){
    std::unique_lock<std::shared_mutex> lock(map_timestamp_mutex);
    map_timestamp[submission] = timestamp;
}

bool plagiarism_checker_t::get_timestamp(std::shared_ptr<submission_t>& key, std::chrono::time_point<std::chrono::steady_clock>& value){
    std::shared_lock<std::shared_mutex> lock(map_timestamp_mutex);
    auto it = map_timestamp.find(key);
    if(it == map_timestamp.end()){
        return false;
    }
    value = it->second;
    return true;
}



// for storing tokens of each submission
void plagiarism_checker_t::insert_tokens(std::shared_ptr<submission_t>& submission, std::vector<int> tokens){
    std::unique_lock<std::shared_mutex> lock(map_tokens_mutex);
    map_tokens[submission] = tokens;
}

bool plagiarism_checker_t::get_tokens(std::shared_ptr<submission_t>& key, std::vector<int>& value){
    std::shared_lock<std::shared_mutex> lock(map_tokens_mutex);
    auto it = map_tokens.find(key);
    if(it == map_tokens.end()){
        return false;
    }
    value = it->second;
    return true;
}


// for storing hash values, submission pointer and starting index

void plagiarism_checker_t::insert_hash(size_t hash, std::shared_ptr<submission_t>& submission, int index){
    std::unique_lock<std::shared_mutex> lock(hash_map_mutex);
    hash_map[hash].push_back(std::make_tuple(submission, index));
}

bool plagiarism_checker_t::get_hash(size_t key, std::vector<std::tuple<std::shared_ptr<submission_t>, int>>& values){
    std::shared_lock<std::shared_mutex> lock(hash_map_mutex);
    auto it = hash_map.find(key);
    if(it == hash_map.end()){
        return false;
    }
    values = it->second;
    return true;
}


plagiarism_checker_t::plagiarism_checker_t(void) {
    // start working thread here
    // should_terminate = false;
    // std::thread working_thread(process_submission());
    // working_thread.detach();
}

plagiarism_checker_t::~plagiarism_checker_t(void) {
    // wait_for_queues_empty();
    while(true){
        bool queues_empty;
        {
            std::scoped_lock lock(submission_queue_mutex,tokenize_queue_mutex);
            queues_empty = submission_queue.empty() && tokenize_queue.empty();
        }
        if(queues_empty){
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    {
        std::scoped_lock lock(submission_queue_mutex,tokenize_queue_mutex);
        should_terminate = true;
    }
    submission_queue_cv.notify_all();
    tokenize_queue_cv.notify_all();

    // Ensure thread cleanup if joinable
    if (submission_thread.joinable()) {
        submission_thread.join();
    }
    if (tokenize_thread.joinable()) {
        tokenize_thread.join();
    }

    // clear all data structures
    {
        std::unique_lock<std::shared_mutex> lock1(map_timestamp_mutex);
        map_timestamp.clear();
    }
    {
        std::unique_lock<std::shared_mutex> lock2(map_tokens_mutex);
        map_tokens.clear();
    }
    { 
        std::unique_lock<std::shared_mutex> lock3(hash_map_mutex);
        hash_map.clear();

    }
}



plagiarism_checker_t::plagiarism_checker_t(std::vector<std::shared_ptr<submission_t>> __submissions) {
    // initialize thread termination flag
    should_terminate = false; 


    for(auto& submission : __submissions){

        if(!submission){
            continue;
        }
        auto zero_time = std::chrono::time_point<std::chrono::steady_clock>();
        insert_timestamp(submission, zero_time);

        tokenizer_t tokenizer(submission->codefile);
        std::vector<int>original_tokens=tokenizer.get_tokens();
        insert_tokens(submission, original_tokens);

        // compute hash here
        compute_rolling_hash(original_tokens, submission);

    }

    tokenize_thread=std::thread(&plagiarism_checker_t::process_tokenization,this);
    submission_thread=std::thread(&plagiarism_checker_t::process_submission,this);
}


void plagiarism_checker_t::process_tokenization(){
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    while(true){  
        std::vector< std::shared_ptr<submission_t> >batch;
        std::chrono::time_point<std::chrono::steady_clock> first_timestamp;

        {
            std::unique_lock<std::mutex> lock(tokenize_queue_mutex);

            tokenize_queue_cv.wait(lock, [this]{
                return !tokenize_queue.empty() || should_terminate;
            });    

            if(should_terminate && tokenize_queue.empty()){
                break;
            }       
            if(!tokenize_queue.empty()){
                auto first_submission=tokenize_queue.front();
                // std::cerr << "Front of the queue: " << first_submission->id << std::endl;
                if(get_timestamp(first_submission,first_timestamp)){
                    batch.push_back(first_submission);
                    tokenize_queue.pop_front();

                    // collect all submissions in 1 sec range
                    while(!tokenize_queue.empty()){

                        auto next_submission=tokenize_queue.front();
                        std::chrono::time_point<std::chrono::steady_clock> next_timestamp;

                        if(get_timestamp(next_submission,next_timestamp) && 
                            next_timestamp-first_timestamp<=std::chrono::seconds(1)){
                            batch.push_back(next_submission);
                            tokenize_queue.pop_front();
                        }else{
                            break;
                        }
                    }
                }
            }
        }

        if(!batch.empty()){
            // process the batch
            for(auto& submission : batch){

                std::vector<int>tokens;

                if(!get_tokens(submission,tokens)){
                    // tokenize
                    tokenizer_t tokenizer(submission->codefile);
                    tokens=tokenizer.get_tokens();
                    insert_tokens(submission,tokens);

                    // compute hash here
                    compute_rolling_hash(tokens, submission);

                }
            }
        }
        {
            std::unique_lock<std::mutex> submission_lock(submission_queue_mutex);
            submission_queue.push_back(batch[0]);
            // std::cerr << "Pushed to submission queue: " << batch[0]->id << std::endl;
        } 
        submission_queue_cv.notify_one();
        if(batch.size()>1){
            std::lock_guard<std::mutex> lock(tokenize_queue_mutex);
            for (int i = batch.size() - 1; i > 0; i--) {
                tokenize_queue.push_front(batch[i]);
                // std::cerr << "Pushed back to tokenize queue: " << batch[i]->id << std::endl;
            }
        } 

        
    }
}


void plagiarism_checker_t::process_submission(){
    while(true){
        std::shared_ptr<submission_t> submission;
        {
            std::unique_lock<std::mutex> lock(submission_queue_mutex);

            submission_queue_cv.wait(lock, [this]{
                return !submission_queue.empty() || should_terminate;
            });

            if(should_terminate && submission_queue.empty()){
                break;
            }

            if(!submission_queue.empty()){
                submission = submission_queue.front();
                // check_plag function
                if(!check_for_plag_1_2(submission)){
                    check_for_plag_3(submission);
                }
                // std::cerr << "checkinggg " << submission->id << std::endl;
                // std::cerr << "Front of the submission queue: " << submission->id << std::endl;       
            }
            submission_queue.pop_front();
        }

    }
}


void plagiarism_checker_t::add_submission(std::shared_ptr<submission_t> __submission) {
    // Insert timestamp immediately for current time
    std::chrono::time_point<std::chrono::steady_clock> current_time = std::chrono::steady_clock::now();
    insert_timestamp(__submission, current_time);

    {
        std::lock_guard<std::mutex> lock(tokenize_queue_mutex);
        tokenize_queue.push_back(__submission);
    }

    // Create a detached thread just for handling the delay and notification
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tokenize_queue_cv.notify_one();
    }).detach();
}

void plagiarism_checker_t::compute_rolling_hash( std::vector<int> &tokens, std::shared_ptr<submission_t> submission,int length){
    
    if (tokens.size() < length) return;
    const size_t base = 257;
    const size_t mod = 1e9 + 7;
    size_t hash = 0;
    size_t base_pow = 1;

    for (int  i= 0; i< length; i++){
        hash = ((hash*base)%mod + (tokens[i]*base)%mod) % mod;
        base_pow = (base_pow * base) % mod;
    }
    insert_hash(hash, submission, 0);

    for (size_t i = length; i < tokens.size(); i++) {
        // Subtract the influence of the outgoing token
        hash = (hash + mod - (tokens[i - length] * base_pow) % mod) % mod;
        // Add the influence of the incoming token
        hash = (((hash + tokens[i]) % mod)*base) % mod;
        insert_hash(hash, submission, i - length + 1);
    }
    return;
}
bool plagiarism_checker_t::check_for_plag_1_2(std::shared_ptr<submission_t> submission){
    std::vector<size_t> hashes;
    std::unordered_map <size_t, std::set<std::tuple<std::shared_ptr<submission_t>,int>>> matched_locations;
    std::unordered_map <std::shared_ptr<submission_t>, int> match_count;
    std::unordered_map <size_t, std::set<std::tuple<std::shared_ptr<submission_t>,int>>> matched_locations2;
    int counter2 = 0;
    int length = 15;
    std::vector<int> tokens;
    get_tokens(submission,tokens);
    if (tokens.size() < length) return false;
    const size_t base = 257;
    const size_t mod = 1e9 + 7;
    size_t hash = 0;
    size_t base_pow = 1;

    for (int  i= 0; i< length; i++){
        hash = (hash*base + tokens[i]*base) % mod;
        base_pow = (base_pow * base) % mod;
        

    }
    // search_for_hash1(hash,submission,matched_locations,match_count);
    // search_for_hash2(hash,submission,matched_locations2,counter2);
    hashes.push_back(hash);

    for (size_t i = length; i < tokens.size(); i++) {
        // Subtract the influence of the outgoing token
        hash = (hash + mod - (tokens[i - length] * base_pow) % mod) % mod;
        // Add the influence of the incoming token
        hash = (((hash + tokens[i]) % mod)*base) % mod;
        // search_for_hash1(hash,submission,matched_locations,match_count);
        // search_for_hash2(hash,submission,matched_locations2,counter2);
        hashes.push_back(hash);
    }
    for(int i=0; i<hashes.size(); i++){
        if(search_for_hash1(hashes[i],submission,matched_locations,match_count)){
            i+=(length-1);
        }
    }
    for(int i=0; i<hashes.size(); i++){
        if(search_for_hash2(hashes[i],submission,matched_locations2,counter2)){
            i+=(length-1);
        }
    }
    if(counter2 >=20){
        if(submission->student){
            student_t student(submission->student->get_name());
            student.flag_student(submission);
        }
        if(submission->professor){
            professor_t professor(submission->professor->get_name());
            professor.flag_professor(submission);
        }
        return true;
    }
    else{
        for(auto &match : match_count){
            if(match.second >= 10){
                if(submission->student){
                    student_t student(submission->student->get_name());
                    student.flag_student(submission);
                }
                if(submission->professor){
                    professor_t professor(submission->professor->get_name());
                    professor.flag_professor(submission);
                }
                return true;
            }
        } 
    }
    return false;
      
}
void plagiarism_checker_t::check_for_plag_3(std::shared_ptr<submission_t> submission){
    int length = 15;
    std::vector<int> tokens;
    get_tokens(submission,tokens);
    if (tokens.size() < length) return;
    const size_t base = 257;
    const size_t mod = 1e9 + 7;
    size_t hash = 0;
    size_t base_pow = 1;

    for (int  i= 0; i< length; i++){
        hash = (hash*base + tokens[i]*base) % mod;
        base_pow = (base_pow * base) % mod;
    }
    if(search_for_hash_3(hash,submission,0)){
        if(submission->student){
            student_t student(submission->student->get_name());
            student.flag_student(submission);
        }
        if(submission->professor){
            professor_t professor(submission->professor->get_name());
            professor.flag_professor(submission);
        }
    }
    for (size_t i = length; i < tokens.size(); i++) {
        // Subtract the influence of the outgoing token
        hash = (hash + mod - (tokens[i - length] * base_pow) % mod) % mod;
        // Add the influence of the incoming token
        hash = (((hash + tokens[i]) % mod)*base) % mod;
        if(search_for_hash_3(hash,submission,i-length+1)){
            if(submission->student){
                student_t student(submission->student->get_name());
                student.flag_student(submission);
            }
            if(submission->professor){
                professor_t professor(submission->professor->get_name());
                professor.flag_professor(submission);
            }
            break;
        }
    }

}
bool plagiarism_checker_t::search_for_hash1(size_t hash, std::shared_ptr<submission_t> submission,std::unordered_map <size_t, std::set<std::tuple<std::shared_ptr<submission_t>, int>>> &matched_locations,std::unordered_map <std::shared_ptr<submission_t>, int> &match_count){
    bool found = false;
    std::vector<std::tuple<std::shared_ptr<submission_t>, int>> all_locations;
    get_hash(hash,all_locations);
    std::unordered_map<std::shared_ptr<submission_t>,bool> checked_submissions;
    for(auto &location : all_locations){
        std::shared_ptr<submission_t> submission2 = std::get<0>(location);
        int index = std::get<1>(location);
        bool within_time_range = false;
        std::chrono::time_point<std::chrono::steady_clock> timestamp1, timestamp2;
        if (get_timestamp(submission, timestamp1) && get_timestamp(submission2, timestamp2)) {
            within_time_range = (timestamp2 <= timestamp1 + std::chrono::seconds(1));
            
        }
        if(submission2 == submission || !within_time_range){
            continue;
        }
        else{
            if(matched_locations[hash].contains(location)){
                continue;
            }
            else if(checked_submissions[submission2]){
                continue;
            }
            else{
                checked_submissions[submission2] = true;
                matched_locations[hash].insert(location);
                match_count[submission2]++;
                found = true;
            }
        }
        
    }
    return found;
}
bool plagiarism_checker_t::search_for_hash2(size_t hash, std::shared_ptr<submission_t> submission,std::unordered_map <size_t, std::set<std::tuple<std::shared_ptr<submission_t>, int>>> &matched_locations2,int &counter){
    bool found = false;
    std::vector<std::tuple<std::shared_ptr<submission_t>, int>> all_locations;
    get_hash(hash,all_locations);
    for(auto &location : all_locations){
        std::shared_ptr<submission_t> submission2 = std::get<0>(location);
        int index = std::get<1>(location);
        bool within_time_range = false;
        std::chrono::time_point<std::chrono::steady_clock> timestamp1, timestamp2;
        if (get_timestamp(submission, timestamp1) && get_timestamp(submission2, timestamp2)) {
            within_time_range = (timestamp2 <= timestamp1 + std::chrono::seconds(1));
            
        }
        if(submission2 == submission || !within_time_range){
            continue;
        }
        else{
            if(matched_locations2[hash].contains(location)){
                continue;
            }
            else{
                matched_locations2[hash].insert(location);
                counter++;
                found = true;
                break;
            }
            
        }
    }
    return found;
}
bool plagiarism_checker_t::search_for_hash_3(size_t hash, std::shared_ptr<submission_t> submission, int index){
    std::vector<std::tuple<std::shared_ptr<submission_t>, int>> all_locations;
    std::vector<int> submission_tokens;

    plagiarism_checker_t::get_tokens(submission,submission_tokens);
    plagiarism_checker_t::get_hash(hash,all_locations);
    for(auto &location : all_locations){
        std::shared_ptr<submission_t> submission2 = std::get<0>(location);
        int index2 = std::get<1>(location);
        bool within_time_range = false;
        std::chrono::time_point<std::chrono::steady_clock> timestamp1, timestamp2;
        if (get_timestamp(submission, timestamp1) && get_timestamp(submission2, timestamp2)) {

            within_time_range = (timestamp2 <= timestamp1 + std::chrono::seconds(1));
            
        }
        if(submission2 == submission || !within_time_range){
            continue;
        }
        else{
            std::vector<int> submission2_tokens;
            plagiarism_checker_t::get_tokens(submission2,submission2_tokens);
            int match_length = 0;
            while(index < submission_tokens.size() && index2 < submission2_tokens.size() && submission_tokens[index] == submission2_tokens[index2] && match_length < 75){
                match_length++;
                index++;
                index2++;
            }
            if(match_length >= 75){
                
                return true;
            }
        }
        
    }
    return false;
}

// End TODO