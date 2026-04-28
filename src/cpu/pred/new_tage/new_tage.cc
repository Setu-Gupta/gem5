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

#include "cpu/pred/new_tage/new_tage.hh"
#include <iostream>
#include <cmath>

namespace gem5
{

	namespace branch_prediction
	{
		New_TAGE::New_TAGE(const New_TAGEParams &params)
			: ConditionalPredictor(params)
			  , nHistoryTables(params.nHistoryTables)
			  , logBaseSize(14) // 16K-entry base predictor
		{
			std::cout << "Initializing TAGE" << '\n';
			global_branch_history.reset();

			//tables.reserve(nHistoryTables);
			basePredictor.resize(1ULL << logBaseSize, GenericSatCounter<int8_t>(2));
			unsigned minHist = params.minHist;
			unsigned maxHist = params.maxHist;

			// tables get history lengths: 5, 13, 32, 80, 200, 640
			for (unsigned i=0; i < nHistoryTables; i++){
				double exponent = (double)i / (double)(nHistoryTables - 1);
				unsigned histLength = (unsigned) (minHist * pow((double)maxHist / minHist, exponent) + 0.5);

				unsigned logNumSets = 10;   // 1024 sets per table
				unsigned assoc = 4;         // 4-way
				unsigned tagSize = 10;      // 10-bit tags

				tables.emplace_back(logNumSets, assoc, histLength, tagSize); 
			}
		}

		bool New_TAGE::lookup(ThreadID tid, Addr PC, void * &bp_history)
		{
			TAGEHistory *history = new TAGEHistory();
			history->global_history = global_branch_history;
			// base prediction
			std::size_t baseIndex = PC & ((1ULL << logBaseSize) - 1);
			bool basePrediction = basePredictor[baseIndex] >= 2;

			history->provider = -1;
			history->alt_provider = -1;


			for (int i = nHistoryTables - 1; i >= 0; i--){
				if (tables[i].hit(PC, global_branch_history)){
					if (history->provider == -1)
						history->provider = i;
					else if (history->alt_provider == -1){
						history->alt_provider = i;
						break;
					}
				}
			}

			if (history->alt_provider >= 0)
				history->alt_prediction = tables[history->alt_provider].predict(PC, global_branch_history);
			else
				history->alt_prediction = basePrediction;

			if (history->provider >= 0) {
				history->hit_tagged_table = true;
				history->provider_prediction = tables[history->provider].predict(PC, global_branch_history);
				history->provider_was_weak = tables[history->provider].isWeak(PC, global_branch_history);

				if (history->provider_was_weak) 
					history->prediction = history->alt_prediction;
				else
					history->prediction = history->provider_prediction;
			}
			else {
				history->hit_tagged_table = false;
				history->provider_prediction = basePrediction;
				history->prediction = basePrediction;
			}
			bp_history = static_cast<void*>(history);
			return history->prediction;
		}

		void New_TAGE::updateHistories(ThreadID tid, Addr PC, bool uncond, bool taken,
				Addr target, const StaticInstPtr &inst,
				void * &bp_history)
		{
			std::cout << "Updating history for " << PC << '\n';
			// If this is an unconditional branch, lookup() was never called,
			// so bp_history is nullptr. We still need to save history
			// for potential squash recovery.
			if (bp_history == nullptr)
			{
				TAGEHistory *history = new TAGEHistory();
				history->global_history = global_branch_history;
				history->provider = -1;
				history->alt_provider = -1;
				history->prediction = true;  // unconditional = always taken
				bp_history = static_cast<void*>(history);
			}

			// Shift global history left by 1
			global_branch_history <<= 1;
			// Push the branch outcome into bit 0
			if (uncond){
				// Unconditional branches are always taken
				global_branch_history[0] = 1;
			}
			else{
				// Conditional: use the predicted direction
				// (this is speculative — may be wrong)
				global_branch_history[0] = taken;
			}
		}

		void New_TAGE::squash(ThreadID tid, void * &bp_history)
		{
			TAGEHistory *history = static_cast<TAGEHistory*>(bp_history);
			global_branch_history = history->global_history;
			delete history;
			bp_history = nullptr;
		}

		void New_TAGE::update(ThreadID tid, Addr PC, bool taken,
				void * &bp_history, bool squashed,
				const StaticInstPtr &inst, Addr target)
		{
			TAGEHistory *history = static_cast<TAGEHistory*>(bp_history);

			// If this branch was on a squashed (wrong) path, don't train
			if (squashed)
			{
				delete history;
				bp_history = nullptr;
				return;
			}

			// Use the history snapshot from prediction time
			// (current global_branch_history has been speculatively updated)
			const auto& gh = history->global_history;

			// ============================================
			// 1. Update base predictor (always)
			// ============================================
			std::size_t baseIndex = PC & ((1ULL << logBaseSize) - 1);
			if (taken)
				basePredictor[baseIndex]++;
			else
				basePredictor[baseIndex]--;

			// ============================================
			// 2. Update provider counter
			// ============================================
			if (history->provider >= 0)
			{
				tables[history->provider].updateCounter(PC, gh, taken);
			}

			// ============================================
			// 3. Update alt provider counter
			//    (only if provider was weak, since we
			//     used alt prediction in that case)
			// ============================================
			if (history->provider_was_weak && history->alt_provider >= 0)
			{
				tables[history->alt_provider].updateCounter(PC, gh, taken);
			}

			// ============================================
			// 4. Manage useful bits
			//    If provider and alt gave DIFFERENT predictions:
			//      - Provider correct → mark useful
			//      - Provider wrong   → mark not useful
			// ============================================
			if (history->provider >= 0)
			{
				if (history->provider_prediction != history->alt_prediction)
				{
					if (history->provider_prediction == taken)
					{
						// Provider was right, alt was wrong → useful
						tables[history->provider].setUseful(PC, gh, true);
					}
					else
					{
						// Provider was wrong, alt was right → not useful
						tables[history->provider].setUseful(PC, gh, false);
					}
				}
			}

			// ============================================
			// 5. On misprediction: allocate in a table
			//    with LONGER history than the provider
			// ============================================
			bool mispredicted = (history->prediction != taken);

			if (mispredicted)
			{
				// Start searching from the table above the provider
				int startTable = history->provider + 1;

				// If no tagged table was hit, start from table 0
				if (history->provider == -1)
					startTable = 0;

				bool allocated = false;

				// Try to allocate in ONE longer-history table
				for (unsigned i = startTable; i < nHistoryTables; i++)
				{
					if (tables[i].allocate(PC, gh, taken))
					{
						allocated = true;
						break;
					}
				}

				// If allocation failed everywhere, age useful bits
				// to make room for future allocations
				if (!allocated)
				{
					for (unsigned i = startTable; i < nHistoryTables; i++)
					{
						tables[i].decrementUseful(PC, gh);
					}
				}
			}

			// ============================================
			// 6. Cleanup
			// ============================================
			delete history;
			bp_history = nullptr;
		}
	} // namespace branch_prediction
} // namespace gem5
