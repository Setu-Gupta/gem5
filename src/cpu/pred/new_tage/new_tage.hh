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
#include <vector>

namespace gem5
{

        namespace branch_prediction
        {
		class New_TAGE : public ConditionalPredictor
                {
                        private:
                                static constexpr std::size_t global_history_length = 1024;
                                std::bitset<global_history_length> global_branch_history;

                                //template<std::size_t counter_width>
                                struct TAGE_Entry
                                {
                                        bool allocated = false;
                                        std::size_t tag;
                                        GenericSatCounter<int8_t> counter;
                                        bool useful;
					
					TAGE_Entry(): counter(4){}
				
					bool predict() const { return counter >= (1 << 3);}
					bool isWeak() const { return (counter == (1 << 3) || counter == ((1 << 3) - 1));}
                                };
				struct TAGEHistory{
					std::bitset<global_history_length> global_history;
					int provider;
					int alt_provider;
					bool provider_prediction;
					bool alt_prediction;
					bool prediction;
					bool provider_was_weak;
					bool hit_tagged_table;
	
					TAGEHistory()
						: global_history(0)
						, provider(-1)
						, alt_provider(-1)
						, provider_prediction(false)
						, alt_prediction(false)
						, prediction(false)
						, provider_was_weak(false)
						, hit_tagged_table(false)
					{};
				};
				
				class TAGE_Table
				{
    					private:
        					unsigned logNumSets;
        					unsigned numSets;
        					unsigned assoc;
        					unsigned histLength;
        					unsigned tagSize;
	
					        std::vector<std::vector<TAGE_Entry>> entries;

        					std::pair<std::size_t, std::size_t> get_tag_index(
        					    Addr PC, const std::bitset<global_history_length>& global_history) const
        					{	
        						const unsigned index_size = logNumSets + tagSize;

					        	std::size_t tag_index = 0;
            						std::size_t slice = 0;
            						unsigned bits_extracted = 0;

            						for (unsigned i = 0; i < histLength; i++)
            						{
                						slice <<= 1;
                						slice |= global_history[i] ? 1ULL : 0ULL;
                						bits_extracted++;

                						if (bits_extracted == index_size)
                						{
                							tag_index ^= slice;
                							slice = 0;
                    							bits_extracted = 0;
                						}
            						}
            						if (bits_extracted > 0)
                						tag_index ^= slice;

            						Addr pc = PC;
            						while (pc != 0)
            						{
                						tag_index ^= pc & ((1ULL << index_size) - 1);
                						pc >>= index_size;
            						}

            						std::size_t index = tag_index & ((1ULL << logNumSets) - 1);
            						std::size_t tag = (tag_index >> logNumSets) & ((1ULL << tagSize) - 1);
            						return {tag, index};
        					}

    					public:
        					TAGE_Table() = default;

        					TAGE_Table(unsigned _logNumSets, unsigned _assoc,
                   					unsigned _histLength, unsigned _tagSize)
            						: logNumSets(_logNumSets)
            						, numSets(1ULL << _logNumSets)
            						, assoc(_assoc)
            						, histLength(_histLength)
            						, tagSize(_tagSize)
            						, entries(1ULL << _logNumSets,
                      					std::vector<TAGE_Entry>(_assoc))
						{}	

                                                bool hit(Addr PC, const std::bitset<1024>& global_history) const
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

                                                bool predict(Addr PC, const std::bitset<1024>& global_history) const
                                                {
                                                        const auto [tag, index] = get_tag_index(PC, global_history);
                                                        const auto& set = entries.at(index);
                                                        for(const auto& entry: set)
                                                        {
                                                                if(entry.allocated && (entry.tag == tag))
                                                                        return entry.predict();
                                                        }
                                                        assert(false && "predict() called without hit");
							return false;
                                                }
	
						bool isWeak(Addr PC, const std::bitset<global_history_length>& gh) const
						{
							const auto [tag, index] = get_tag_index(PC, gh);
							const auto& set = entries.at(index);
							for (const auto& entry : set)
							{
								if (entry.allocated && entry.tag == tag)
									return entry.isWeak();
							}
							assert(false && "isWeak() called without hit");
							return false;
						}
						
						void updateCounter(Addr PC,
							const std::bitset<global_history_length>& gh, bool taken)
						{
							const auto [tag, index] = get_tag_index(PC, gh);
							auto& set = entries.at(index);
							for (auto& entry : set)
							{
								if (entry.allocated && entry.tag == tag)
        							{
            								if (taken)
                								entry.counter++;
            								else
            									entry.counter--;
								return;
								}
							}
						}

						// ============================================
						// Set or clear the useful bit for a matching entry
						// ============================================
						void setUseful(Addr PC, const std::bitset<global_history_length>& gh, bool val)
						{
							const auto [tag, index] = get_tag_index(PC, gh);
							auto& set = entries.at(index);
							for (auto& entry : set)
							{
								if (entry.allocated && entry.tag == tag)
								{
									entry.useful = val;
									return;
								}
							}
						}

						// ============================================
						// Try to allocate a new entry
						// Returns true if allocation succeeded
						// ============================================
						bool allocate(Addr PC,
								const std::bitset<global_history_length>& gh, bool taken)
						{
							const auto [tag, index] = get_tag_index(PC, gh);
							auto& set = entries.at(index);

							// First: look for an unallocated slot
							for (auto& entry : set)
							{
								if (!entry.allocated)
								{
									entry.allocated = true;
									entry.tag = tag;
									entry.counter.reset();
									// Weak taken or weak not-taken
									if (taken)
										entry.counter++;
									entry.useful = false;
									return true;
								}
							}

							// Second: look for a not-useful slot to evict
							for (auto& entry : set)
							{
								if (!entry.useful)
								{
									entry.allocated = true;
									entry.tag = tag;
									entry.counter.reset();
									if (taken)
										entry.counter++;
									entry.useful = false;
									return true;
								}
							}

							// All entries are useful — allocation fails
							return false;
						}

						// ============================================
						// Age (clear) useful bits in the indexed set
						// Called when allocation fails, to make room
						// for future allocations
						// ============================================
						void decrementUseful(Addr PC,
								const std::bitset<global_history_length>& gh)
						{
							const auto [tag, index] = get_tag_index(PC, gh);
							auto& set = entries.at(index);
							for (auto& entry : set)
							{
								entry.useful = false;
							}
						}
				};
				unsigned nHistoryTables;
				std::vector<TAGE_Table> tables;
				std::vector<GenericSatCounter<int8_t>> basePredictor;
				unsigned logBaseSize;
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
		}; // class New_Tage
	} // namespace branch_prediction
} // namespace gem5

#endif //  __CPU_PRED_BI_MODE_PRED_HH__
