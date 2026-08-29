#include <array>
#include <iostream>
#include <span>
#include <vector>

// -----------------------------------------------------------------------------
#include <unordered_map>
#include <unordered_set>
#include <cmath>




/*
 * compute_rolling_hashes -  Computes rolling hashes for all length-grams in a submission.
 * 
 * submission - The submission vector.
 * length - The length of the gram
 * return -  An unordered_map from hash to a vector containing starting indices of corresponding of length-grams.
 */
std::unordered_map< size_t, std::vector<int> > compute_rolling_hashes(const std::vector<int> &submission, int length) {
    // hash_map is used to store the hash values and the starting index of the substring
    std::unordered_map<size_t, std::vector<int> > hash_map;
    
    if (submission.size() < length) return hash_map;

    const size_t base = 257;
    const size_t mod = 1e9 + 7;
    size_t hash = 0;
    size_t base_pow = 1;

    // Compute the hash for the first length-gram
    for (int i = 0; i < length; ++i) {
        hash = (hash * base + submission[i]) % mod;
        if (i < length - 1) {
            base_pow = (base_pow * base) % mod;
        }
    }

    hash_map[hash].push_back(0);

    for (size_t i = length; i < submission.size(); ++i) {
        // Subtract the influence of the outgoing token
        hash = (hash + mod - (submission[i - length] * base_pow) % mod) % mod;
        // Add the influence of the incoming token
        hash = (hash * base + submission[i]) % mod;
        hash_map[hash].push_back(i - length + 1);
    }
    return hash_map;
}




/*
 * find_matches -  Finds exact matches between two submissions.
 * 
 * submission1 - The first submission vector.
 * submission2 - The second submission vector.
 * length - The minimum length of the substring.
 * match_indices -  A vector to store the corresponding match indices and match length.
 * return - The total exact match length.
 */

int find_matches(const std::vector<int> &submission1, const std::vector<int> &submission2, int length, std::vector<std::tuple<int, int, int>> &match_indices) {
    // visited1 and visited2 are used to keep track of the visited tokens in submission1 and submission2
    std::unordered_map<int, bool> visited1;
    std::unordered_map<int, bool> visited2;

    int total_match_length = 0;
    auto hash_map1 = compute_rolling_hashes(submission1, length);

    const size_t base = 257;
    const size_t mod = 1e9 + 7;
    size_t hash = 0;
    size_t base_pow = 1;
    // hash value for the first length-gram in submission2
    for (int i = 0; i < length; ++i) {
        hash = (hash * base + submission2[i]) % mod;
        if (i > 0) base_pow = (base_pow * base) % mod;
    }

    // Sliding window to compute hash values for reamining length-grams
    for (size_t i = 0; i <= submission2.size() - length; ++i) {
        if (i > 0) {
            // Update the hash for the new window
            hash = (hash + mod - (submission2[i - 1] * base_pow) % mod) % mod;
            hash = (hash * base + submission2[i + length - 1]) % mod;
        }

        // Check if this hash exists in `submission1`
        if (hash_map1.find(hash) != hash_map1.end()) {

            // select at most 1 starting index from submission1
            auto indices = hash_map1[hash];
            for (int start_idx1 : indices) {
                // Check if the match is already visited
                if (visited1[start_idx1] || visited2[i]) {
                    continue;
                }
                int match_length = 0;
                // Verify the match by comparing characters
                while (start_idx1 + match_length < submission1.size() &&
                       i + match_length < submission2.size() && 
                       submission1[start_idx1 + match_length] == submission2[i + match_length]) {
                    
                    if (visited1[start_idx1 + match_length] || visited2[i + match_length]) {
                        break; // Break immediately if we are trying to match a visited element
                    }
                    match_length++;
                }

                // If the match length is greater than 10, store the match indices and match length and mark the elements as visited
                if (match_length >= 10) {
                    match_indices.push_back({start_idx1, i,match_length});
                    for (int k = 0; k < match_length; ++k) {
                        visited1[start_idx1 + k] = true;
                        visited2[i + k] = true;
                    }
                    total_match_length += match_length;
                }
                break;
            }
        }
    }
    return total_match_length;
}



/*
 * find_maximum_approximate_matches - Finds maximum approximate match between two submissions.
 * 
 * This function iterates over all the matches found in the previous step and extends them
 * forward and backward, allowing for gaps in both submission1 and submission2. 
 * It returns the length and starting indices of the longest approximate match.
 *
 * submission1 - The first submission vector.
 * submission2 - The second submission vector.
 * match_indices - A vector of match indices from the previous step.
 * gap_fraction -  The allowed fraction of gaps in the match.
 * return -  A tuple containing the length of the longest match, the starting index in submission1, and the starting index in submission2.
 */


std::tuple<int, int, int> find_maximum_approximate_matches(const std::vector<int> &submission1, const std::vector<int> &submission2, const std::vector<std::tuple<int, int, int>> &match_indices, double gap_fraction=0.05) {
    int max_length = 0;
    int max_start_idx1 = -1;
    int max_start_idx2 = -1;

    for (const auto& [start_idx1, start_idx2, initial_match] : match_indices) {
        int forward_match_length1 = initial_match; // it is the length of match in forward direction from the starting index of an exact match in submission1
        int forward_match_length2 = initial_match; // it is the length of match in forward direction from the starting index of an exact match in submission2
        int backward_match_length1 = 0; // it is the length of match in backward direction from the starting index of an exact match in submission1
        int backward_match_length2 = 0; // it is the length of match in backward direction from the starting index of an exact match in submission2
        int total_gaps1 = 0;
        int total_gaps2 = 0;
        int current_start_idx1 = start_idx1; // this is used to track the starting index of approximate match in submission1
        int current_start_idx2 = start_idx2; // this is used to track the starting index of approximate match in submission2

        bool expanded = true;

        while (expanded) {
            expanded = false;
            
            int current_total_length = std::max(forward_match_length1 + backward_match_length1,
                                              forward_match_length2 + backward_match_length2);
            int max_gaps_allowed = gap_fraction * current_total_length;
            
            // Backward expansion
            while (current_start_idx1 > 0 && current_start_idx2 > 0 && 
                   total_gaps1 < max_gaps_allowed && total_gaps2 < max_gaps_allowed) {
                bool found_match = false;
                
                // Case 1: Try gap in submission1 only
                for (int gap1 = 0; gap1 + total_gaps1 <= max_gaps_allowed && 
                     current_start_idx1 - gap1 > 0; gap1++) {
                    if (submission1[current_start_idx1 - gap1 - 1] == 
                        submission2[current_start_idx2 - 1]) {
                        current_start_idx1 -= (gap1 + 1);
                        current_start_idx2 -= 1;
                        backward_match_length1 += (gap1 + 1);
                        backward_match_length2 += 1;
                        total_gaps1 += gap1;
                        found_match = true;
                        expanded = true;
                        break;
                    }
                }
                
                if (!found_match) {
                    // Case 2: Try gap in submission2 only
                    for (int gap2 = 0; gap2 + total_gaps2 <= max_gaps_allowed && 
                         current_start_idx2 - gap2 > 0; gap2++) {
                        if (submission1[current_start_idx1 - 1] == 
                            submission2[current_start_idx2 - gap2 - 1]) {
                            current_start_idx1 -= 1;
                            current_start_idx2 -= (gap2 + 1);
                            backward_match_length1 += 1;
                            backward_match_length2 += (gap2 + 1);
                            total_gaps2 += gap2;
                            found_match = true;
                            expanded = true;
                            break;
                        }
                    }
                }

                if (!found_match) break;
            }

            // Recalculate allowed gaps after backward expansion
            current_total_length = std::max(forward_match_length1 + backward_match_length1,
                                          forward_match_length2 + backward_match_length2);
            max_gaps_allowed = gap_fraction * current_total_length;

            // Forward expansion
            while (start_idx1 + forward_match_length1 < submission1.size() && 
                   start_idx2 + forward_match_length2 < submission2.size() &&
                   total_gaps1 < max_gaps_allowed && total_gaps2 < max_gaps_allowed) {
                
                bool found_match = false;
                
                // Case 1: Try gap in submission1 only
                for (int gap1 = 0; gap1 + total_gaps1 <= max_gaps_allowed && 
                     start_idx1 + forward_match_length1 + gap1 < submission1.size(); gap1++) {
                    if (submission1[start_idx1 + forward_match_length1 + gap1] == 
                        submission2[start_idx2 + forward_match_length2]) {
                        forward_match_length1 += (gap1 + 1);
                        forward_match_length2 += 1;
                        total_gaps1 += gap1;
                        found_match = true;
                        expanded = true;
                        break;
                    }
                }
                
                if (!found_match) {
                    // Case 2: Try gap in submission2 only
                    for (int gap2 = 0; gap2 + total_gaps2 <= max_gaps_allowed && 
                         start_idx2 + forward_match_length2 + gap2 < submission2.size(); gap2++) {
                        if (submission1[start_idx1 + forward_match_length1] == 
                            submission2[start_idx2 + forward_match_length2 + gap2]) {
                            forward_match_length1 += 1;
                            forward_match_length2 += (gap2 + 1);
                            total_gaps2 += gap2;
                            found_match = true;
                            expanded = true;
                            break;
                        }
                    }
                }

                if (!found_match) break;
            }
        }

        // Rest of the alignment code remains the same
        while (current_start_idx1 < submission1.size() && current_start_idx2 < submission2.size() &&
               submission1[current_start_idx1] != submission2[current_start_idx2]) {
            current_start_idx1++;
            current_start_idx2++;
            total_gaps1--;
            total_gaps2--;
            backward_match_length1--;
            backward_match_length2--;
        }
        
        while (current_start_idx1 + forward_match_length1 + backward_match_length1 - 1 < submission1.size() &&
               current_start_idx2 + forward_match_length2 + backward_match_length2 - 1 < submission2.size() &&
               submission1[current_start_idx1 + forward_match_length1 + backward_match_length1 - 1] != 
               submission2[current_start_idx2 + forward_match_length2 + backward_match_length2 - 1]) {
            forward_match_length1--;
            forward_match_length2--;
            total_gaps1--;
            total_gaps2--;
        }

        int current_length = std::min(forward_match_length1 + backward_match_length1,
                                    forward_match_length2 + backward_match_length2);
        
        if (current_length >= 30 && current_length > max_length) {
            max_length = current_length;
            max_start_idx1 = current_start_idx1;
            max_start_idx2 = current_start_idx2;
        }
    }

    return {max_length, max_start_idx1, max_start_idx2};
}



std::array<int, 5> match_submissions(std::vector<int> &submission1, std::vector<int> &submission2) {
    std::array<int, 5> result = {0, 0, 0, 0, 0};

    int length = 5; 
    if (submission1.size() < length || submission2.size() < length) {
        return result;
    }

    std::vector<std::tuple<int, int, int>> match_indices;

    int total_match_length=0;
    int max_length=0;
    int match_idx_1=-1;
    int match_idx_2=-1;
    double required_match_percentage = 0.40;
    double gap_fraction = 0.20;
    int exact;

    std::tuple<int,int, int> approximate;

    // two cases based on the size of the submissions
    if(submission1.size()>submission2.size()){
        exact = find_matches(submission1, submission2, length,match_indices);
        approximate=find_maximum_approximate_matches(submission1, submission2,match_indices,gap_fraction);
        match_idx_1 = std::get<1>(approximate);
        match_idx_2 = std::get<2>(approximate);
        max_length = std::get<0>(approximate);
    }else{
        exact = find_matches(submission2, submission1, length,match_indices);
        approximate=find_maximum_approximate_matches(submission2, submission1,match_indices,gap_fraction);
        match_idx_2 = std::get<1>(approximate);
        match_idx_1 = std::get<2>(approximate);
        max_length = std::get<0>(approximate);
    }

    total_match_length=exact;

    // Check if the total match length is greater than the required percentage of the minimum length
    if (total_match_length >= required_match_percentage * std::min(submission1.size(), submission2.size())) {
        result[0] = 1;
    }
    result[1] = total_match_length;
    result[2] = max_length;
    result[3] = match_idx_1; 
    result[4] = match_idx_2; 

    // std::cout << "Result: " << result[0] << " " << result[1] << " " << result[2] << " " << result[3] << " " << result[4] << "  sub1: "<<submission1.size()<<"  sub2: "<<submission2.size()<<std::endl;
    return result;
}