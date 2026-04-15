/*
 * Copyright (c) 2022-2023 The University of Edinburgh
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2014 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* @file
 * Implementation of a bi-mode branch predictor
 */

#ifndef __CPU_PRED_NEW_TAGE_PRED_HH__
#define __CPU_PRED_NEW_TAGE_PRED_HH__

#include "base/sat_counter.hh"
#include "cpu/pred/conditional.hh"
#include "params/New_TAGE.hh"
#include <bitset>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <map>
#include <iostream>
#include <utility>

namespace gem5
{

        namespace branch_prediction
        {
                class New_TAGE : public ConditionalPredictor
                {
                        private:
                                static constexpr std::size_t global_history_length = 1024;
                                std::bitset<global_history_length> global_branch_history;

                                template<std::size_t counter_width>
                                struct TAGE_Entry
                                {
                                        bool allocated = false;
                                        std::size_t tag;
                                        GenericSatCounter<int8_t> counter{counter_width};
                                        const std::size_t max_counter_value = (1ULL << counter_width);
                                        bool useful;
                                };

                                template<std::size_t log2_num_sets, std::size_t associativity, std::size_t msb, std::size_t lsb, std::size_t tag_size>
                                class TAGE_Table
                                {
                                        private:
                                                std::array<std::array<TAGE_Entry<4>, associativity>, (1ULL << log2_num_sets)> entries;

                                                std::pair<std::size_t, std::size_t> get_tag_index(Addr PC, const std::bitset<global_history_length> &global_history) const
                                                {
                                                        constexpr std::size_t index_size = tag_size + log2_num_sets;
                                                        static_assert(index_size <= sizeof(std::size_t), "The table index values and tag, combined together, are limited to the size of std::size_t");

                                                        // Extract the relevant bits from global history and fold them over
                                                        std::size_t tag_index = 0;
                                                        std::size_t extracted_slice = 0;
                                                        std::size_t num_bits_extracted = 0;
                                                        for(std::size_t i = lsb; i <= msb; i++)
                                                        {
                                                                extracted_slice << 1;
                                                                if(global_history[i])
                                                                        extracted_slice ^= 1ULL;
                                                                else
                                                                        extracted_slice ^= 0ULL;
                                                                num_bits_extracted++;
                                                                if(num_bits_extracted == index_size)
                                                                {
                                                                        tag_index ^= extracted_slice;
                                                                        extracted_slice = 0ULL;
                                                                        num_bits_extracted = 0;
                                                                }
                                                        }

                                                        while(PC != 0)
                                                        {
                                                                tag_index ^= PC & ((1ULL << index_size) - 1);
                                                                PC >> index_size;
                                                        }

                                                        const std::size_t index_mask = ((1ULL << log2_num_sets) - 1ULL);
                                                        const std::size_t index = tag_index & index_mask;
                                                        const std::size_t tag_mask = ~((1ULL << log2_num_sets) - 1ULL);
                                                        const std::size_t tag = (tag_index & ~index_mask) >> log2_num_sets;

                                                        return {tag, index};
                                                }

                                        public:
                                                bool hit(Addr PC, std::bitset<1024> global_history) const
                                                {
                                                        const auto [tag, index] = get_tag_index(PC, global_history);
                                                        const auto& set = entries.at(index);
                                                        for(const auto& entry: set)
                                                        {
                                                                if(entry.allocated && (entry.tag == tag))
                                                                        return true;
                                                        }
                                                        return false;
                                                }

                                                bool predict(Addr PC, std::bitset<1024> global_history) const
                                                {
                                                        const auto [tag, index] = get_tag_index(PC, global_history);
                                                        const auto& set = entries.at(index);
                                                        for(const auto& entry: set)
                                                        {
                                                                if(entry.allocated && (entry.tag == tag))
                                                                        return entry.counter >= (entry.max_counter_value >> 1);
                                                        }
                                                        assert(false);
                                                }
                                };
                        public:
                                New_TAGE(const New_TAGEParams &params);
                                bool lookup(ThreadID tid, Addr PC, void * &bp_history) override;
                                void updateHistories(ThreadID tid, Addr PC, bool uncond, bool taken,
                                                Addr target, const StaticInstPtr &inst,
                                                void * &bp_history) override;
                                void squash(ThreadID tid, void * &bp_history) override;
                                void update(ThreadID tid, Addr PC, bool taken,
                                                void * &bp_history, bool squashed,
                                                const StaticInstPtr & inst, Addr target) override;
                };

        } // namespace branch_prediction
} // namespace gem5

#endif //  __CPU_PRED_BI_MODE_PRED_HH__
