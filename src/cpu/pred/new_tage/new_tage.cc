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

namespace gem5
{

        namespace branch_prediction
        {
                New_TAGE::New_TAGE(const New_TAGEParams &params)
                        : ConditionalPredictor(params)
                {
                        std::cout << "Initializing TAGE" << '\n';
                }

                bool New_TAGE::lookup(ThreadID tid, Addr PC, void * &bp_history)
                {
                        std::cout << "Predicting branch at " << PC << '\n';
                        return false;
                }

                void New_TAGE::updateHistories(ThreadID tid, Addr PC, bool uncond, bool taken,
                                Addr target, const StaticInstPtr &inst,
                                void * &bp_history)
                {
                        std::cout << "Updating history for " << PC << '\n';
                }

                void New_TAGE::squash(ThreadID tid, void * &bp_history)
                {
                        std::cout << "Squashing\n";
                }

                void New_TAGE::update(ThreadID tid, Addr PC, bool taken, void * &bp_history,
                                bool squashed, const StaticInstPtr & inst, Addr target)
                {
                        std::cout << "Updating for " << PC << '\n';
                }

        } // namespace branch_prediction
} // namespace gem5
