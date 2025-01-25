// Copyright 2024 Eren Balatkan. All Rights Reserved.

#include "MissNoHit.h"
#include "MnhTracer.h"
#include "MnhTracerComponent.h"
#include "Async/ParallelFor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealEngine.h"

#define LOCTEXT_NAMESPACE "FMissNoHitModule"

void FMissNoHitModule::StartupModule()
{
	this->TracerDatas.Reserve(4096);
}

void FMissNoHitModule::ShutdownModule()
{
}


void FMissNoHitModule::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_MnhTickTracers);
	
	TickIdx++;
	// Lock removals so we don't get any modifications to the array while we are iterating
	RemovalLock = true;
	
	UpdateTracerTransforms(DeltaTime);
	PerformTraces(DeltaTime);
	NotifyTraceResults();

	// Reverse iterate, remove pending removals
	RemovalLock = false;
	for (int TracerDataIdx = TracerDatas.Num() - 1; TracerDataIdx >= 0; TracerDataIdx--)
	{
		const auto& TracerData = TracerDatas[TracerDataIdx];
		if (!TracerDatas.IsValidIndex(TracerDataIdx))
		{
			continue;
		}
		
		if (TracerData.IsPendingRemoval)
		{
			RemoveTracerDataAt(TracerDataIdx, TracerData.Guid);
		}
	}
}

void FMissNoHitModule::RemoveTracerDataAt(const int TracerDataIdx, const FGuid Guid)
{
	SCOPE_CYCLE_COUNTER(STAT_MnhRemoveTracer)
	if (TracerDatas.IsValidIndex(TracerDataIdx) && TracerDatas[TracerDataIdx].Guid == Guid)
	{
		FScopeLock ScopeLock(&CriticalSection);
		TracerDatas.RemoveAtSwap(TracerDataIdx);

		// If we removed the last element in array we don't need to update the index of the tracer
		if (TracerDatas.IsValidIndex(TracerDataIdx))
		{
			const auto TracerData = TracerDatas[TracerDataIdx];
			if (TracerData.bUsesTracerConfig)
			{
				TracerData.OwnerTracerComponent->TracerConfigs[TracerData.OwnerTracerConfigIdx].TracerDataIdx = TracerDataIdx;
			}
			else
			{
				TracerDatas[TracerDataIdx].OwnerTracer->TracerDataIdx = TracerDataIdx;
			}
		}
	}
}

void FMissNoHitModule::UpdateTracerTransforms(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_MnhTracerUpdateTransforms);
	ParallelFor(TracerDatas.Num(), [&](const int32 TracerDataIdx)
	{
		if (!TracerDatas.IsValidIndex(TracerDataIdx)){
			return;
		}
		auto& TracerData = TracerDatas[TracerDataIdx];
		if (TracerData.TracerState == EMnhTracerState::Stopped)
		{
			return;
		}
		
		if(!IsValid(TracerData.SourceComponent))
		{
			TracerData.TracerState = EMnhTracerState::Stopped;
			TracerData.bShouldTickThisFrame = false;
			return;
		}
		
		check(TracerData.bShouldTickThisFrame == false)
		
		const auto CurrentTransform = TracerData.GetCurrentTracerTransform();
		
		if (TracerData.TracerTransformsOverTime.Num() == 0)
		{
			TracerData.TracerTransformsOverTime.Add(CurrentTransform);
		}
		
		if (TracerData.TracerState == EMnhTracerState::PendingStop)
		{
			TracerData.bShouldTickThisFrame = true;
			TracerData.TracerTransformsOverTime.Add(CurrentTransform);
			return;
		}
		
		switch (TracerData.TracerTickType)
		{
		case EMnhTracerTickType::MatchGameTick:
			TracerData.TracerTransformsOverTime.Add(CurrentTransform);
			TracerData.bShouldTickThisFrame = true;
			return;
		case EMnhTracerTickType::DistanceTick:
			if ((TracerData.TracerTransformsOverTime[0].GetLocation() - CurrentTransform.GetLocation()).Length() >= TracerData.TickInterval)
			{
				TracerData.TracerTransformsOverTime.Add(CurrentTransform);
				TracerData.bShouldTickThisFrame = true;
			}
			return;
		case EMnhTracerTickType::FixedRateTick:
			if (TracerData.DeltaTimeLastTick + DeltaTime > TracerData.TickInterval / 2){
				TracerData.TracerTransformsOverTime.Add(CurrentTransform);
				TracerData.bShouldTickThisFrame = true;
			}
			return;
		}
	});
}

void FMissNoHitModule::PerformTraces(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_MnhTracerDoTrace)

	ParallelFor(TracerDatas.Num(), [&](const int32 TracerDataIdx)
	{
		if (!TracerDatas.IsValidIndex(TracerDataIdx)){
			return;
		}
		auto& TracerData = TracerDatas[TracerDataIdx];
		if (TracerData.bShouldTickThisFrame)
		{
			int SubSteps = 1;
			if (TracerData.TracerTickType == EMnhTracerTickType::DistanceTick)
			{
				SubSteps = FMath::CeilToInt((TracerData.TracerTransformsOverTime[0].GetLocation() - TracerData.TracerTransformsOverTime[1].GetLocation()).Length() / TracerData.TickInterval);
			}
			else if (TracerData.TracerTickType == EMnhTracerTickType::FixedRateTick)
			{
				SubSteps = FMath::CeilToInt(DeltaTime / TracerData.TickInterval);
			}
			SubSteps = FMath::Min(10, SubSteps);
			TracerData.DoTrace(SubSteps, TickIdx);
		}
		
		TracerData.DeltaTimeLastTick += DeltaTime;
	});
}

void FMissNoHitModule::NotifyTraceResults()
{
	for (auto& TracerData : TracerDatas)
	{
		if (!TracerData.bShouldTickThisFrame)
		{
			continue;
		}
		
		const auto OwnerTracer = TracerData.OwnerTracer;
		for (const auto& SubstepResults : TracerData.SubstepHits)
		{
			// Respect cancellations by user-defined code immediately.
			if (TracerData.TracerState == EMnhTracerState::Stopped)
			{
				break;
			}

			if (TracerData.bUsesTracerConfig)
			{
				auto& TracerConfig = TracerData.OwnerTracerComponent->TracerConfigs[TracerData.OwnerTracerConfigIdx];
				if (TracerConfig.DrawDebugType != EDrawDebugTrace::None)
				{
					FMnhHelpers::DrawDebug(SubstepResults.StartLocation,
						SubstepResults.EndLocation,
						SubstepResults.Scale,
						SubstepResults.Rotation,
						SubstepResults.HitResults, TracerData.ShapeData, TracerData.World,
						TracerConfig.DrawDebugType,
						TracerConfig.DrawDebugType == EDrawDebugTrace::ForOneFrame ? TracerData.DeltaTimeLastTick : TracerConfig.DebugDrawTime,
						TracerConfig.DebugTraceColor, TracerConfig.DebugTraceBlockColor, TracerConfig.DebugTraceHitColor);
				}

				TracerData.OwnerTracerComponent->OnTracerHitDetected(TracerConfig.TracerTag, SubstepResults.HitResults,
					TracerData.DeltaTimeLastTick / TracerData.SubstepHits.Num(), TickIdx);
			}
			else
			{
				if (TracerData.OwnerTracer->DrawDebugType != EDrawDebugTrace::None)
				{
					FMnhHelpers::DrawDebug(SubstepResults.StartLocation,
						SubstepResults.EndLocation,
						SubstepResults.Scale,
						SubstepResults.Rotation,
						SubstepResults.HitResults, TracerData.ShapeData, TracerData.World,
						OwnerTracer->DrawDebugType,
						OwnerTracer->DrawDebugType == EDrawDebugTrace::ForOneFrame ? TracerData.DeltaTimeLastTick : OwnerTracer->DebugDrawTime,
						OwnerTracer->DebugTraceColor, OwnerTracer->DebugTraceBlockColor, OwnerTracer->DebugTraceHitColor);
				}
			
				TracerData.OwnerTracerComponent->OnTracerHitDetected(TracerData.OwnerTracer->TracerTag, SubstepResults.HitResults,
					TracerData.DeltaTimeLastTick / TracerData.SubstepHits.Num(), TickIdx);
			}
		}
			
		TracerData.DeltaTimeLastTick = 0;
		TracerData.bShouldTickThisFrame = false;
	
		if (TracerData.TracerState == EMnhTracerState::PendingStop)
		{
			TracerData.TracerState = EMnhTracerState::Stopped;
			TracerData.TracerTransformsOverTime.Empty();
		}
		else
		{
			TracerData.TracerTransformsOverTime.RemoveAtSwap(0);
		}
	}
}

void FMissNoHitModule::MarkTracerDataForRemoval(const int TracerDataIdx, const FGuid Guid)
{
	if (RemovalLock)
	{
		if (TracerDatas.IsValidIndex(TracerDataIdx) && TracerDatas[TracerDataIdx].Guid == Guid)
		{
			TracerDatas[TracerDataIdx].IsPendingRemoval = true;
		}
	}
	else
	{
		RemoveTracerDataAt(TracerDataIdx, Guid);
	}
	
}

int FMissNoHitModule::RequestNewTracerData()
{
	FScopeLock ScopeLock(&CriticalSection);
	return TracerDatas.AddDefaulted();
}

UWorld* FMissNoHitModule::GetTickableGameObjectWorld() const
{
	// Hacky way to get the world
	if (TracerDatas.Num() > 0)
	{
		return TracerDatas[0].World;
	}
	return nullptr;
}

FMnhTracerData& FMissNoHitModule::GetTracerDataAt(const int Index)
{
	return TracerDatas[Index];
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMissNoHitModule, MissNoHit)