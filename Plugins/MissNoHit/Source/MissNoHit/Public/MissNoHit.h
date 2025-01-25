// Copyright 2024 Eren Balatkan. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Tickable.h"
#include "Engine/HitResult.h"
#include "MissNoHit.generated.h"

struct FMnhTracerData;
class UMnhTracer;

USTRUCT()
struct FMnhMultiTraceResultContainer
{
	GENERATED_BODY()
public:
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	FVector Scale = FVector::OneVector;
	FQuat Rotation = FQuat::Identity;
	TArray<FHitResult> HitResults;
};

class FMissNoHitModule : public IModuleInterface, public FTickableGameObject
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool IsGameModule() const override { return true; }
	
	//FTickableGameObject implementation
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override {return false;}
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override { return TStatId(); }

	FCriticalSection CriticalSection;
	FMnhTracerData& GetTracerDataAt(int Index);

	int RequestNewTracerData();
	void MarkTracerDataForRemoval(int TracerDataIdx, FGuid Guid);

private:
	TArray<FMnhTracerData> TracerDatas;
	bool RemovalLock = false;
	uint32 TickIdx = 0;
	void RemoveTracerDataAt(int TracerDataIdx, FGuid Guid);

	void UpdateTracerTransforms(const float DeltaTime);
	void PerformTraces(const float DeltaTime);
	void NotifyTraceResults();
};
