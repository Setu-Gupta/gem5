# Copyright (c) 2022-2023 The University of Edinburgh
# Copyright (c) 2024 Technical University of Munich
# Copyright (c) 2025 Arm Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2012 Mark D. Hill and David A. Wood
# Copyright (c) 2015 The University of Wisconsin
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from m5.objects.ClockedObject import ClockedObject
from m5.objects.IndexingPolicies import *
from m5.objects.ReplacementPolicies import *
from m5.params import *
from m5.proxy import *
from m5.SimObject import *
from m5.objects.BranchPredictor import ConditionalPredictor


class TAGEBase_v2(SimObject):
    type = "TAGEBase_v2"
    cxx_class = "gem5::branch_prediction::TAGEBase_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_base.hh"

    numThreads = Param.Unsigned(Parent.numThreads, "Number of threads")
    instShiftAmt = Param.Unsigned(
        Parent.instShiftAmt, "Number of bits to shift instructions by"
    )

    nHistoryTables = Param.Unsigned(7, "Number of history tables")
    minHist = Param.Unsigned(5, "Minimum history size of TAGE")
    maxHist = Param.Unsigned(130, "Maximum history size of TAGE")

    tagTableTagWidths = VectorParam.Unsigned(
        [0, 9, 9, 10, 10, 11, 11, 12], "Tag size in TAGE tag tables"
    )
    logTagTableSizes = VectorParam.Int(
        [13, 9, 9, 9, 9, 9, 9, 9], "Log2 of TAGE table sizes"
    )
    logRatioBiModalHystEntries = Param.Unsigned(
        2,
        "Log num of prediction entries for a shared hysteresis bit "
        "for the Bimodal",
    )

    tagTableCounterBits = Param.Unsigned(3, "Number of tag table counter bits")
    tagTableUBits = Param.Unsigned(2, "Number of tag table u bits")

    histBufferSize = Param.Unsigned(
        2097152,
        "A large number to track all branch histories(2MEntries default)",
    )

    pathHistBits = Param.Unsigned(16, "Path history size")
    logUResetPeriod = Param.Unsigned(
        18, "Log period in number of branches to reset TAGE useful counters"
    )
    numUseAltOnNa = Param.Unsigned(1, "Number of USE_ALT_ON_NA counters")
    initialTCounterValue = Param.Int(1 << 17, "Initial value of tCounter")
    useAltOnNaBits = Param.Unsigned(4, "Size of the USE_ALT_ON_NA counter(s)")

    maxNumAlloc = Param.Unsigned(
        1, "Max number of TAGE entries allocted on mispredict"
    )

    # List of enabled TAGE tables. If empty, all are enabled
    noSkip = VectorParam.Bool([], "Vector of enabled TAGE tables")

    speculativeHistUpdate = Param.Bool(
        Parent.speculativeHistUpdate, "Use speculative update for histories"
    )

    # Taken only history as used in most modern server CPUs.
    takenOnlyHistory = Param.Bool(
        Parent.takenOnlyHistory,
        "Build the global history only from taken branches (2-bit) "
        "instead of direction history from all branches. Widely implemented "
        "in modern server CPUs: https://ieeexplore.ieee.org/document/9246215",
    )


class StatisticalCorrector_v2(SimObject):
    type = "StatisticalCorrector_v2"
    cxx_class = "gem5::branch_prediction::StatisticalCorrector_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/statistical_corrector.hh"
    abstract = True

    instShiftAmt = Param.Unsigned(
        Parent.instShiftAmt, "Number of bits to shift instructions by"
    )

    # Statistical corrector parameters

    numEntriesFirstLocalHistories = Param.Unsigned(
        "Number of entries for first local histories"
    )

    bwnb = Param.Unsigned("Num global backward branch GEHL lengths")
    bwm = VectorParam.Int("Global backward branch GEHL lengths")
    logBwnb = Param.Unsigned("Log num of global backward branch GEHL entries")
    bwWeightInitValue = Param.Int(
        "Initial value of the weights of the global backward branch GEHL entries"
    )

    lnb = Param.Unsigned("Num first local history GEHL lenghts")
    lm = VectorParam.Int("First local history GEHL lengths")
    logLnb = Param.Unsigned("Log number of first local history GEHL entries")
    lWeightInitValue = Param.Int(
        "Initial value of the weights of the first local history GEHL entries"
    )

    inb = Param.Unsigned(1, "Num IMLI GEHL lenghts")
    im = VectorParam.Int([8], "IMLI history GEHL lengths")
    logInb = Param.Unsigned("Log number of IMLI GEHL entries")
    iWeightInitValue = Param.Int(
        "Initial value of the weights of the IMLI history GEHL entries"
    )

    logBias = Param.Unsigned("Log size of Bias tables")

    logSizeUp = Param.Unsigned(
        6, "Log size of update threshold counters tables"
    )

    chooserConfWidth = Param.Unsigned(
        7, "Number of bits for the chooser counters"
    )

    updateThresholdWidth = Param.Unsigned(
        12, "Number of bits for the update threshold counter"
    )

    pUpdateThresholdWidth = Param.Unsigned(
        8, "Number of bits for the pUpdate threshold counters"
    )

    extraWeightsWidth = Param.Unsigned(
        6, "Number of bits for the extra weights"
    )

    scCountersWidth = Param.Unsigned(6, "Statistical corrector counters width")

    initialUpdateThresholdValue = Param.Int(
        0, "Initial pUpdate threshold counter value"
    )

    speculativeHistUpdate = Param.Bool(
        Parent.speculativeHistUpdate,
        "Use speculative update for the statistical corrector",
    )


class LoopPredictor_v2(SimObject):
    type = "LoopPredictor_v2"
    cxx_class = "gem5::branch_prediction::LoopPredictor_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/loop_predictor.hh"

    logSizeLoopPred = Param.Unsigned(8, "Log size of the loop predictor")
    withLoopBits = Param.Unsigned(7, "Size of the WITHLOOP counter")
    loopTableAgeBits = Param.Unsigned(8, "Number of age bits per loop entry")
    loopTableConfidenceBits = Param.Unsigned(
        2, "Number of confidence bits per loop entry"
    )
    loopTableTagBits = Param.Unsigned(14, "Number of tag bits per loop entry")
    loopTableIterBits = Param.Unsigned(14, "Nuber of iteration bits per loop")
    logLoopTableAssoc = Param.Unsigned(2, "Log loop predictor associativity")

    # Parameters for enabling modifications to the loop predictor
    # They have been copied from TAGE-GSC-IMLI
    # (http://www.irisa.fr/alf/downloads/seznec/TAGE-GSC-IMLI.tar)
    #
    # All of them should be disabled to match the original LTAGE implementation
    # (http://hpca23.cse.tamu.edu/taco/camino/cbp2/cbp-src/realistic-seznec.h)

    # Add speculation
    useSpeculation = Param.Bool(False, "Use speculation")

    # Add hashing for calculating the loop table index
    useHashing = Param.Bool(False, "Use hashing")

    # Add a direction bit to the loop table entries
    useDirectionBit = Param.Bool(False, "Use direction info")

    # If true, use random to decide whether to allocate or not, and only try
    # with one entry
    restrictAllocation = Param.Bool(
        False, "Restrict the allocation conditions"
    )

    initialLoopIter = Param.Unsigned(1, "Initial iteration number")
    initialLoopAge = Param.Unsigned(255, "Initial age value")
    optionalAgeReset = Param.Bool(
        True, "Reset age bits optionally in some cases"
    )


# TAGE branch predictor as described in https://www.jilp.org/vol8/v8paper1.pdf
# The default sizes below are for the 8C-TAGE configuration (63.5 Kbits)
class TAGE_v2(ConditionalPredictor):
    type = "TAGE_v2"
    cxx_class = "gem5::branch_prediction::TAGE_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage.hh"

    tage = Param.TAGEBase_v2(TAGEBase_v2(), "Tage object")


class LTAGE_TAGE_v2(TAGEBase_v2):
    nHistoryTables = 12
    minHist = 4
    maxHist = 640
    tagTableTagWidths = [0, 7, 7, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15]
    logTagTableSizes = [14, 10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 9, 9]
    logUResetPeriod = 19


# LTAGE branch predictor as described in
# https://www.irisa.fr/caps/people/seznec/L-TAGE.pdf
# It is basically a TAGE predictor plus a loop predictor
# The differnt TAGE sizes are updated according to the paper values (256 Kbits)
class LTAGE_v2(TAGE_v2):
    type = "LTAGE_v2"
    cxx_class = "gem5::branch_prediction::LTAGE_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/ltage.hh"

    tage = LTAGE_TAGE_v2()

    loop_predictor = Param.LoopPredictor_v2(LoopPredictor_v2(), "Loop predictor")


# TAGE-SC-L branch predictor as desribed in
# https://www.jilp.org/cbp2016/paper/AndreSeznecLimited.pdf
# It is a modified LTAGE predictor plus a statistical corrector predictor
# The TAGE modifications include bank interleaving and partial associativity
# Two different sizes are proposed in the paper:
# 8KB => See TAGE_SC_L_8KB below
# 64KB => See TAGE_SC_L_64KB below
# The TAGE_SC_L_8KB and TAGE_SC_L_64KB classes differ not only on the values
# of some parameters, but also in some implementation details
# Given this, the TAGE_SC_L class is left abstract
# Note that as it is now, this branch predictor does not handle any type
# of speculation: All the structures/histories are updated at commit time
class TAGE_SC_L_v2(LTAGE_v2):
    type = "TAGE_SC_L_v2"
    cxx_class = "gem5::branch_prediction::TAGE_SC_L_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_sc_l.hh"
    abstract = True

    statistical_corrector = Param.StatisticalCorrector_v2(
        "Statistical Corrector. Set to NULL to disable it"
    )


class TAGE_SC_L_LoopPredictor_v2(LoopPredictor_v2):
    type = "TAGE_SC_L_LoopPredictor_v2"
    cxx_class = "gem5::branch_prediction::TAGE_SC_L_LoopPredictor_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_sc_l.hh"

    loopTableAgeBits = 4
    loopTableConfidenceBits = 4
    loopTableTagBits = 10
    loopTableIterBits = 10
    useSpeculation = False
    useHashing = True
    useDirectionBit = True
    restrictAllocation = True
    initialLoopIter = 0
    initialLoopAge = 7
    optionalAgeReset = False


class TAGE_SC_L_64KB_LoopPredictor_v2(TAGE_SC_L_LoopPredictor_v2):
    logSizeLoopPred = 5


class TAGE_SC_L_TAGE_v2(TAGEBase_v2):
    type = "TAGE_SC_L_TAGE_v2"
    cxx_class = "gem5::branch_prediction::TAGE_SC_L_TAGE_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_sc_l.hh"
    abstract = True

    tagTableTagWidths = [0]
    numUseAltOnNa = 16
    pathHistBits = 27
    maxNumAlloc = 2
    logUResetPeriod = 10
    initialTCounterValue = 1 << 9
    useAltOnNaBits = 5

    # This size does not set the final sizes of the tables (it is just used
    # for some calculations)
    # Instead, the number of TAGE entries comes from shortTagsTageEntries and
    # longTagsTageEntries
    logTagTableSize = Param.Unsigned("Log size of each tag table")

    shortTagsTageFactor = Param.Unsigned(
        "Factor for calculating the total number of short tags TAGE entries"
    )

    longTagsTageFactor = Param.Unsigned(
        "Factor for calculating the total number of long tags TAGE entries"
    )

    shortTagsSize = Param.Unsigned(8, "Size of the short tags")

    longTagsSize = Param.Unsigned("Size of the long tags")

    firstLongTagTable = Param.Unsigned("First table with long tags")

    truncatePathHist = Param.Bool(
        True, "Truncate the path history to its configured size"
    )


class TAGE_SC_L_TAGE_64KB_v2(TAGE_SC_L_TAGE_v2):
    type = "TAGE_SC_L_TAGE_64KB_v2"
    cxx_class = "gem5::branch_prediction::TAGE_SC_L_TAGE_64KB_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_sc_l_64KB.hh"

    nHistoryTables = 36

    minHist = 6
    maxHist = 3000

    tagTableUBits = 1

    logTagTableSizes = [13]

    # This is used to handle the 2-way associativity
    # (all odd entries are set to one, and if the corresponding even entry
    # is set to one, then there is a 2-way associativity for this pair)
    # Entry 0 is for the bimodal and it is ignored
    # Note: For this implementation, some odd entries are also set to 0 to save
    # some bits
    noSkip = [
        0,
        0,
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        1,
    ]

    logTagTableSize = 10
    shortTagsTageFactor = 10
    longTagsTageFactor = 20

    longTagsSize = 12

    firstLongTagTable = 13


class TAGE_SC_L_64KB_StatisticalCorrector_v2(StatisticalCorrector_v2):
    type = "TAGE_SC_L_64KB_StatisticalCorrector_v2"
    cxx_class = "gem5::branch_prediction::TAGE_SC_L_64KB_StatisticalCorrector_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_sc_l_64KB.hh"

    pnb = Param.Unsigned(3, "Num variation global branch GEHL lengths")
    pm = VectorParam.Int([25, 16, 9], "Variation global branch GEHL lengths")
    logPnb = Param.Unsigned(
        9, "Log number of variation global branch GEHL entries"
    )

    snb = Param.Unsigned(3, "Num second local history GEHL lenghts")
    sm = VectorParam.Int([16, 11, 6], "Second local history GEHL lengths")
    logSnb = Param.Unsigned(
        9, "Log number of second local history GEHL entries"
    )

    tnb = Param.Unsigned(2, "Num third local history GEHL lenghts")
    tm = VectorParam.Int([9, 4], "Third local history GEHL lengths")
    logTnb = Param.Unsigned(
        10, "Log number of third local history GEHL entries"
    )

    imnb = Param.Unsigned(2, "Num second IMLI GEHL lenghts")
    imm = VectorParam.Int([10, 4], "Second IMLI history GEHL lengths")
    logImnb = Param.Unsigned(9, "Log number of second IMLI GEHL entries")

    numEntriesSecondLocalHistories = Param.Unsigned(
        16, "Number of entries for second local histories"
    )
    numEntriesThirdLocalHistories = Param.Unsigned(
        16, "Number of entries for second local histories"
    )

    numEntriesFirstLocalHistories = 256

    logBias = 8

    bwnb = 3
    bwm = [40, 24, 10]
    logBwnb = 10
    bwWeightInitValue = 7

    lnb = 3
    lm = [11, 6, 3]
    logLnb = 10
    lWeightInitValue = 7

    logInb = 8
    iWeightInitValue = 7


# 64KB TAGE-SC-L branch predictor as described in
# http://www.jilp.org/cbp2016/paper/AndreSeznecLimited.pdf
class TAGE_SC_L_64KB_v2(TAGE_SC_L_v2):
    type = "TAGE_SC_L_64KB_v2"
    cxx_class = "gem5::branch_prediction::TAGE_SC_L_64KB_v2"
    cxx_header = "cpu/pred/tage_sc_l_v2/tage_sc_l_64KB.hh"

    tage = TAGE_SC_L_TAGE_64KB_v2()
    loop_predictor = TAGE_SC_L_64KB_LoopPredictor_v2()
    statistical_corrector = TAGE_SC_L_64KB_StatisticalCorrector_v2()
