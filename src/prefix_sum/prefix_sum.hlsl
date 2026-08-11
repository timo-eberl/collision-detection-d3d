#define BLOCK_DIM           256U
#define UINT4_PER_THREAD    3U
#define MIN_WAVE_SIZE       4U
#define UINT4_PART_SIZE     (BLOCK_DIM * UINT4_PER_THREAD)

#define FLAG_NOT_READY      0
#define FLAG_REDUCTION      1
#define FLAG_INCLUSIVE      2
#define FLAG_MASK           3

cbuffer cbConstants : register(b0) {
	uint e_vectorizedSize;
	uint e_threadBlocks;
};

RWStructuredBuffer<uint4> b_scanIn               : register(u0);
RWStructuredBuffer<uint4> b_scanOut              : register(u1);
globallycoherent RWStructuredBuffer<uint> b_scanBump             : register(u2);
globallycoherent RWStructuredBuffer<uint> b_threadBlockReduction : register(u3);

groupshared uint g_reduction[BLOCK_DIM / MIN_WAVE_SIZE];
groupshared uint g_broadcast;

struct t_scan {
	uint4 t[UINT4_PER_THREAD];
};

#ifdef OP_MAX
	#define SCAN_OP(a, b) max((a), (b))
	#define WAVE_ACTIVE_OP(x) WaveActiveMax(x)
	#define SCAN_IDENTITY 0
#else
	#define SCAN_OP(a, b) ((a) + (b))
	#define WAVE_ACTIVE_OP(x) WaveActiveSum(x)
	#define SCAN_IDENTITY 0
#endif

inline uint getWaveIndex(uint gtid) {
	return gtid / WaveGetLaneCount();
}

inline uint PartStart(uint partIndex) {
	return partIndex * UINT4_PART_SIZE;
}

inline uint WavePartSize() {
	return UINT4_PER_THREAD * WaveGetLaneCount();
}

inline uint WavePartStart(uint gtid) {
	return getWaveIndex(gtid) * WavePartSize();
}

// Emulates WavePrefixMax for architectures that only support WavePrefixSum
inline uint WavePrefixOp(uint val) {
#ifdef OP_MAX
	uint res = val;
	uint lane = WaveGetLaneIndex();
	
	// Unroll through max potential wave size (64)
	[unroll]
	for (uint offset = 1; offset < 64; offset *= 2) {
		uint neighbor = WaveReadLaneAt(res, lane - offset);
		if (offset < WaveGetLaneCount() && lane >= offset) {
			res = max(res, neighbor);
		}
	}
	
	uint excl = WaveReadLaneAt(res, lane - 1);
	return lane == 0 ? 0 : excl;
#else
	return WavePrefixSum(val);
#endif
}

inline void AcquirePartitionIndex(uint gtid) {
	if (!gtid) InterlockedAdd(b_scanBump[0], 1, g_broadcast);
}

inline void DeviceBroadcast(uint gtid, uint partIndex) {
	if (!gtid) {
		uint payload = SCAN_OP(SCAN_IDENTITY, g_reduction[BLOCK_DIM / WaveGetLaneCount() - 1]);
		uint t;
		InterlockedExchange(b_threadBlockReduction[partIndex],
			(partIndex ? FLAG_REDUCTION : FLAG_INCLUSIVE) | (payload << 2), t);
	}
}

inline void LookbackSingle(uint partIndex) {
	uint prevReduction = SCAN_IDENTITY;
	uint lookBackIndex = partIndex - 1;
	
	while (true) {
		uint flagPayload = b_threadBlockReduction[lookBackIndex];

		if ((flagPayload & FLAG_MASK) > FLAG_NOT_READY) {
			prevReduction = SCAN_OP(prevReduction, flagPayload >> 2);
			if ((flagPayload & FLAG_MASK) == FLAG_INCLUSIVE) {
				uint payload = SCAN_OP(prevReduction, g_reduction[BLOCK_DIM / WaveGetLaneCount() - 1]);
				uint t;
				InterlockedExchange(b_threadBlockReduction[partIndex], 
									FLAG_INCLUSIVE | (payload << 2), t);
				g_broadcast = prevReduction;
				break;
			} else {
				lookBackIndex--;
			}
		}
	}
}

inline void SpineScan(uint gtid) {
	uint laneLog = countbits(WaveGetLaneCount() - 1);
	uint spineSize = BLOCK_DIM >> laneLog;
	uint alignedSize = 1U << (countbits(spineSize - 1) + laneLog - 1) / laneLog * laneLog;
	uint offset = 0;
	
	for (uint j = WaveGetLaneCount(); j <= alignedSize; j <<= laneLog) {
		uint t0 = j != WaveGetLaneCount() ? 1 : 0;
		uint i0 = (gtid + (t0 << offset)) - t0;
		bool pred0 = i0 < spineSize;
		
		uint t1 = pred0 ? g_reduction[i0] : SCAN_IDENTITY;
		uint t2 = SCAN_OP(t1, WavePrefixOp(t1));
		
		if (pred0) g_reduction[i0] = t2;
		GroupMemoryBarrierWithGroupSync();
		
		if (j != WaveGetLaneCount()) {
			uint rshift = j >> laneLog;
			uint i1 = gtid + rshift;
			if ((i1 & (j - 1)) >= rshift) {
				bool pred1 = i1 < spineSize;
				uint t3 = pred1 ? g_reduction[((i1 >> offset) << offset) - 1] : SCAN_IDENTITY;
				if (pred1 && ((i1 + 1) & (rshift - 1)) != 0) {
					g_reduction[i1] = SCAN_OP(g_reduction[i1], t3);
				}
			}
		}
		offset += laneLog;
	}
}

#ifndef OP_MAX
inline void ScanExclusiveSum(uint gtid, uint partIndex, inout t_scan t_s, bool isPartial) {
	uint laneMask = WaveGetLaneCount() - 1;
	uint circularShift = WaveGetLaneIndex() + laneMask & laneMask;
	uint waveReduction = 0;
	
	[unroll]
	for (uint i = WaveGetLaneIndex() + WavePartStart(gtid) + PartStart(partIndex), k = 0;
		 k < UINT4_PER_THREAD;
		 i += WaveGetLaneCount(), ++k) {
		
		t_s.t[k] = (!isPartial || i < e_vectorizedSize) ? b_scanIn[i] : 0;
		
		uint t0 = t_s.t[k].x;
		t_s.t[k].x += t_s.t[k].y;
		t_s.t[k].y = t0;

		t0 = t_s.t[k].x;
		t_s.t[k].x += t_s.t[k].z;
		t_s.t[k].z = t0;

		t0 = t_s.t[k].x;
		t_s.t[k].x += t_s.t[k].w;
		t_s.t[k].w = t0;
		
		uint t1 = WaveReadLaneAt(t_s.t[k].x + WavePrefixSum(t_s.t[k].x), circularShift);
		uint localBase = (WaveGetLaneIndex() ? t1 : 0) + waveReduction;
		
		t_s.t[k] = uint4(localBase, t_s.t[k].yzw + localBase);
		waveReduction += WaveReadLaneAt(t1, 0);
	}
	
	if (!WaveGetLaneIndex()) g_reduction[getWaveIndex(gtid)] = waveReduction;
}

inline void PropagateSum(uint gtid, uint partIndex, uint prevReduction, t_scan t_s, bool isPartial) {
	for (uint i = WaveGetLaneIndex() + WavePartStart(gtid) + PartStart(partIndex), k = 0;
		 k < UINT4_PER_THREAD && (!isPartial || i < e_vectorizedSize);
		 i += WaveGetLaneCount(), ++k) {
		b_scanOut[i] = t_s.t[k] + prevReduction;
	}
}

#else
inline void ScanInclusiveMax(uint gtid, uint partIndex, inout t_scan t_s, bool isPartial) {
	uint laneMask = WaveGetLaneCount() - 1;
	uint circularShift = WaveGetLaneIndex() + laneMask & laneMask;
	uint waveReduction = 0;
	
	[unroll]
	for (uint i = WaveGetLaneIndex() + WavePartStart(gtid) + PartStart(partIndex), k = 0;
		 k < UINT4_PER_THREAD;
		 i += WaveGetLaneCount(), ++k) {
		
		t_s.t[k] = (!isPartial || i < e_vectorizedSize) ? b_scanIn[i] : 0;
		
		t_s.t[k].y = max(t_s.t[k].y, t_s.t[k].x);
		t_s.t[k].z = max(t_s.t[k].z, t_s.t[k].y);
		t_s.t[k].w = max(t_s.t[k].w, t_s.t[k].z);
		
		uint t = WaveReadLaneAt(max(t_s.t[k].w, WavePrefixOp(t_s.t[k].w)), circularShift);
		uint laneOffset = WaveGetLaneIndex() ? t : 0;
		uint combinedOffset = max(laneOffset, waveReduction);
		
		t_s.t[k] = max(t_s.t[k], uint4(combinedOffset, combinedOffset, combinedOffset, combinedOffset));
		waveReduction = max(waveReduction, WaveReadLaneAt(t, 0));
	}
	
	if (!WaveGetLaneIndex()) g_reduction[getWaveIndex(gtid)] = waveReduction;
}

inline void PropagateMax(uint gtid, uint partIndex, uint prevReduction, t_scan t_s, bool isPartial) {
	for (uint i = WaveGetLaneIndex() + WavePartStart(gtid) + PartStart(partIndex), k = 0;
		 k < UINT4_PER_THREAD && (!isPartial || i < e_vectorizedSize);
		 i += WaveGetLaneCount(), ++k) {
		uint4 prev = uint4(prevReduction, prevReduction, prevReduction, prevReduction);
		b_scanOut[i] = max(t_s.t[k], prev);
	}
}
#endif

[numthreads(256, 1, 1)]
void InitChainedScan(uint3 id : SV_DispatchThreadID) {
	uint gidFlat = id.x + id.y * 65535;
	if (gidFlat < e_threadBlocks) b_threadBlockReduction[gidFlat] = 0;
	if (gidFlat == 0) b_scanBump[0] = 0;
}

[numthreads(BLOCK_DIM, 1, 1)]
void ChainedScanDecoupledLookback(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
	uint gidFlat = gid.x + gid.y * 65535;
	if (gidFlat >= e_threadBlocks) return;
	
	AcquirePartitionIndex(gtid.x);
	GroupMemoryBarrierWithGroupSync();
	uint partitionIndex = g_broadcast;

	t_scan t_s;
	bool isPartial = (partitionIndex == e_threadBlocks - 1);
	
#ifndef OP_MAX
	ScanExclusiveSum(gtid.x, partitionIndex, t_s, isPartial);
#else
	ScanInclusiveMax(gtid.x, partitionIndex, t_s, isPartial);
#endif

	GroupMemoryBarrierWithGroupSync();
	SpineScan(gtid.x);
	GroupMemoryBarrierWithGroupSync();
	
	DeviceBroadcast(gtid.x, partitionIndex);
	
	if (partitionIndex && !gtid.x) LookbackSingle(partitionIndex);
	GroupMemoryBarrierWithGroupSync();
	
	uint waveRed = gtid.x >= WaveGetLaneCount() ? g_reduction[getWaveIndex(gtid.x) - 1] : SCAN_IDENTITY;
	uint prevReduction = SCAN_OP(g_broadcast, waveRed);
	
#ifndef OP_MAX
	PropagateSum(gtid.x, partitionIndex, prevReduction, t_s, isPartial);
#else
	PropagateMax(gtid.x, partitionIndex, prevReduction, t_s, isPartial);
#endif
}
