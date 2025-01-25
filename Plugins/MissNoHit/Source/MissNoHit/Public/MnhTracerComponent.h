// Copyright 2024 Eren Balatkan. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MnhHelpers.h"
#include "MnhTracer.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "MnhTracerComponent.generated.h"

struct FGameplayTagContainer;
class UMnhTracer;
struct FGameplayTag;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMnhOnHitDetected, FGameplayTag, TracerTag, FHitResult, HitResult, float, DeltaTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMnhOnTraceFinished, FGameplayTag, TracerTag, const TArray<FHitResult>&, HitResult, const float, DeltaTime);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMnhOnTracerStopped, FGameplayTag, TracerTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMnhOnTracerStarted, FGameplayTag, TracerTag);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMnhTracerComponentDestroyed);

struct FMnhHitCache
{
	FHitResult HitResult;
	FGameplayTag TracerTag;
	int TickIdx;
};

struct FTracerInitializationData
{
	FGameplayTagContainer TracerTags;
	UPrimitiveComponent* TracerSource;
};

UENUM(BlueprintType)
enum class EMnhFilterType : uint8
{
    FilterSameActorAcrossAllTracers UMETA(DisplayName="FilterSameActorAcrossAllTracers"),
    FilterSameActorPerTracer UMETA(DisplayName="FilterSameActorPerTracer"),
	None UMETA(DisplayName="None")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent),
	HideCategories=(Sockets, Navigation, Tags, ComponentTick, ComponentReplication,
		Cooking, AssetUserData, Replication),
		meta=(PrioritizeCategories="Mnh Hit Tracers"), CollapseCategories)
class MISSNOHIT_API UMnhTracerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMnhTracerComponent();
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	UPROPERTY(BlueprintAssignable)
	FMnhOnHitDetected OnHitDetected;
	
	FMnhOnTracerStarted OnTracerStarted;
	FMnhOnTracerStopped OnTracerStopped;
	FMnhTracerComponentDestroyed OnDestroyed;

	FCriticalSection TraceDoneScopeLock;

	TArray<FMnhHitCache> HitCache;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="MissNoHit", meta=(FullyExpand=true, TitleProperty="TracerTag"))
	TArray<FMnhTracerConfig> TracerConfigs;
	
	/* DEPRECATED, USE TRACE CONFIG INSTEAD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MissNoHit", meta=(FullyExpand=true))
	TArray<TObjectPtr<UMnhTracer>> Tracers;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	EMnhFilterType FilterType;

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void InitializeTracers(const FGameplayTagContainer TracerTags, UPrimitiveComponent* TracerSource);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void StopAllTracers();

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void ResetHitCache();

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddToIgnoredActors(FGameplayTagContainer TracerTags, AActor* Actor);
	
	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddToIgnoredActorsArray(const FGameplayTagContainer TracerTags, TArray<AActor*> Actors);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void StartTracers(FGameplayTagContainer TracerTags, bool bResetHitCache=true);

	void StartTracersInternal(const FGameplayTagContainer& TracerTags, bool bResetHitCache, bool AllowAnimNotify);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void StopTracers(FGameplayTagContainer TracerTags);

	void StopTracersDelayed(const FGameplayTagContainer& TracerTags);

	UMnhTracer* FindTracer(FGameplayTag TracerTag);
	int FindTracerConfig(FGameplayTag TracerTag);
	
	int AddNewTracer(const FGameplayTag TracerTag, const EMnhTraceSource TraceSource, const FMnhTraceSettings& TraceSettings,
		EMnhTracerTickType TracerTickType, int TargetFps, int TargetDistanceTraveled,
		EDrawDebugTrace::Type DrawDebugType, float DrawDebugLifetime=0.5);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddNewMnhComponentTracer(const FGameplayTag TracerTag,
		FMnhTraceSettings TraceSettings, EMnhTracerTickType TracerTickType, int TargetFps, int TargetDistanceTraveled,
		EDrawDebugTrace::Type DrawDebugType, float DrawDebugLifetime=0.5);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddNewPhysicsAssetTracer(const FGameplayTag TracerTag,
		FName SocketOrBoneName,
		FMnhTraceSettings TraceSettings, EMnhTracerTickType TracerTickType, int TargetFps, int TargetDistanceTraveled,
		EDrawDebugTrace::Type DrawDebugType, float DrawDebugLifetime=0.5);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddNewAnimNotifyTracer(const FGameplayTag TracerTag,
		FMnhTraceSettings TraceSettings, EMnhTracerTickType TracerTickType, int TargetFps, int TargetDistanceTraveled,
		EDrawDebugTrace::Type DrawDebugType, float DrawDebugLifetime=0.5);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddNewStaticMeshSocketsTracer(const FGameplayTag TracerTag,
		FName MeshSocket1, FName MeshSocket2, float MeshSocketTracerRadius, float MeshSocketTracerLengthOffset,
		FMnhTraceSettings TraceSettings, EMnhTracerTickType TracerTickType, int TargetFps, int TargetDistanceTraveled,
		EDrawDebugTrace::Type DrawDebugType, float DrawDebugLifetime=0.5);

	UFUNCTION(BlueprintCallable, Category="MissNoHit")
	void AddNewSkeletalMeshSocketsTracer(const FGameplayTag TracerTag,
		FName MeshSocket1, FName MeshSocket2, float MeshSocketTracerRadius, float MeshSocketTracerLengthOffset,
		FMnhTraceSettings TraceSettings, EMnhTracerTickType TracerTickType, int TargetFps, int TargetDistanceTraveled,
		EDrawDebugTrace::Type DrawDebugType, float DrawDebugLifetime=0.5);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual bool CheckFilters(const FHitResult& HitResult, FGameplayTag TracerTag, int TickIdxArg);

public:
	void OnTracerHitDetected(FGameplayTag TracerTag, const TArray<FHitResult>& HitResults, const float DeltaTime, const int TickIdx);

private:
	bool bIsInitialized = false;
	TArray<FTracerInitializationData> EarlyTracerInitializations;
};
