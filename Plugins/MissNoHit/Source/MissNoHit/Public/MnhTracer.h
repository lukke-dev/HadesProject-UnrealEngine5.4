// Copyright 2024 Eren Balatkan, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MissNoHit.h"
#include "MnhHelpers.h"
#include "UObject/Object.h"
#include "Kismet/KismetSystemLibrary.h"
#include "WorldCollision.h"
#include "MnhTracer.generated.h"

struct FMnhTracerData;
class UMnhTraceType;
class UMnhTracerComponent;
struct FGameplayTag;
struct FMnhTraceSettings;
class UMnhHitFilter;

/**
 * 
 */
UCLASS(DefaultToInstanced, EditInlineNew, CollapseCategories, ShowCategories=(TracerBase, TracerType), meta=(FullyExpand=true))
class MISSNOHIT_API UMnhTracer : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UMnhTracer() override;

	/* Identifier for the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	FGameplayTag TracerTag;

	/* An enumeration that specifies the source of the tracer. It can be a shape component, a physics asset, or an animation notification.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	EMnhTraceSource TraceSource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	FName MeshSocket_1 = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	FName MeshSocket_2 = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	float MeshSocketTracerRadius = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	float MeshSocketTracerLengthOffset = 0;

	/* Name of the Socket or Bone Tracer will be attached to */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::SocketOrBone)||(TraceSource==EMnhTraceSource::PhysicsAsset)", EditConditionHides))
	FName SocketOrBoneName;

	/* Shape specifications of the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="false", EditConditionHides))
	FMnhShapeData ShapeData;

	/* Collision settings for the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	FMnhTraceSettings TraceSettings;

	/* Specifies how the tracer should tick. Sub-stepping is disabled on Match Game Tick */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	EMnhTracerTickType TracerTickType;

	/* Tick Rate of the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="TracerTickType==EMnhTracerTickType::FixedRateTick", EditConditionHides))
	int TargetFps = 30;

	/* Distance traveled by Tracer between each Tick */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="TracerTickType==EMnhTracerTickType::DistanceTick", EditConditionHides))
	int TickDistanceTraveled = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug")
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
		meta=(EditCondition="DrawDebugType!=EDrawDebugTrace::None", EditConditionHides))
	FColor DebugTraceColor = FColor::Red;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
		meta=(EditCondition="DrawDebugType!=EDrawDebugTrace::None", EditConditionHides))
	FColor DebugTraceHitColor = FColor::Green;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
			meta=(EditCondition="DrawDebugType!=EDrawDebugTrace::None", EditConditionHides))
	FColor DebugTraceBlockColor = FColor::Blue; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
		meta=(EditCondition="DrawDebugType==EDrawDebugTrace::ForDuration", EditConditionHides))
	float DebugDrawTime = 0.5;

	UPROPERTY(BlueprintReadOnly, Category="MissNoHit")
	TObjectPtr<UPrimitiveComponent> SourceComponent;

	bool bIsTracerActive = false;
	
	TObjectPtr<UMnhTracerComponent> OwnerComponent;
	
	FCollisionQueryParams CollisionParams;
	FCollisionObjectQueryParams ObjectQueryParams;

	void InitializeParameters(UPrimitiveComponent* SourceComponentArg);
	void InitializeFromPhysicAsset();
	void InitializeFromCollisionShape();
	void InitializeFromStaticMeshSockets();
	void InitializeFromSkeletalMeshSockets();
	bool IsTracerActive() const;
	
	void ChangeTracerState(bool bIsTracerActiveArg, bool bStopImmediate=true);

	int TracerDataIdx;
	FGuid TracerDataGuid;

	void RegisterTracerData();
	void UpdateTracerData();
	void MarkTracerDataForRemoval() const;

private:
	FMnhTracerData& GetTracerData() const;
	
};

USTRUCT(BlueprintType)
struct FMnhTracerConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	FGameplayTag TracerTag;
	
	UPROPERTY(EditAnywhere, Category="MissNoHit")
	EMnhTraceSource TraceSource = EMnhTraceSource::MnhShapeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	FName MeshSocket_1 = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	FName MeshSocket_2 = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	float MeshSocketTracerRadius = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::StaticMeshSockets||TraceSource==EMnhTraceSource::SkeletalMeshSockets)", EditConditionHides))
	float MeshSocketTracerLengthOffset = 0;

	/* Name of the Socket or Bone Tracer will be attached to */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="(TraceSource==EMnhTraceSource::SocketOrBone)||(TraceSource==EMnhTraceSource::PhysicsAsset)", EditConditionHides))
	FName SocketOrBoneName;

	/* Shape specifications of the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="false", EditConditionHides))
	FMnhShapeData ShapeData;

	/* Collision settings for the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	FMnhTraceSettings TraceSettings;

	/* Specifies how the tracer should tick. Sub-stepping is disabled on Match Game Tick */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit")
	EMnhTracerTickType TracerTickType = EMnhTracerTickType::MatchGameTick;

	/* Tick Rate of the Tracer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="TracerTickType==EMnhTracerTickType::FixedRateTick", EditConditionHides))
	int TargetFps = 30;

	/* Distance traveled by Tracer between each Tick */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit",
		meta=(EditCondition="TracerTickType==EMnhTracerTickType::DistanceTick", EditConditionHides))
	int TickDistanceTraveled = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug")
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
		meta=(EditCondition="DrawDebugType!=EDrawDebugTrace::None", EditConditionHides))
	FColor DebugTraceColor = FColor::Red;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
		meta=(EditCondition="DrawDebugType!=EDrawDebugTrace::None", EditConditionHides))
	FColor DebugTraceHitColor = FColor::Green;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
			meta=(EditCondition="DrawDebugType!=EDrawDebugTrace::None", EditConditionHides))
	FColor DebugTraceBlockColor = FColor::Blue; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MissNoHit|Debug",
		meta=(EditCondition="DrawDebugType==EDrawDebugTrace::ForDuration", EditConditionHides))
	float DebugDrawTime = 0.5;
	
	int TracerDataIdx = -1;
	FGuid TracerDataGuid;
	bool bIsTracerActive;
	
	TObjectPtr<UPrimitiveComponent> SourceComponent;
	int OwnerTracerConfigIdx;
	TObjectPtr<UMnhTracerComponent> OwnerTracerComponent;

	FCollisionQueryParams CollisionParams;
	FCollisionObjectQueryParams ObjectQueryParams;
	
	void InitializeParameters(UPrimitiveComponent* SourceComponentArg);
	void InitializeFromPhysicAsset();
	void InitializeFromCollisionShape();
	void InitializeFromStaticMeshSockets();
	void InitializeFromSkeletalMeshSockets();

	void ChangeTracerState(bool bIsTracerActiveArg, bool bStopImmediate=true);
	bool IsTracerActive() const;
	void RegisterTracerData();
	void UpdateTracerData() const;
	void MarkTracerDataForRemoval() const;

private:
	FMnhTracerData& GetTracerData() const;
};

USTRUCT(BlueprintType)
struct FMnhTracerData
{
	GENERATED_BODY()

public:
	EMnhTraceSource TraceSource;
	FName MeshSocket_1 = NAME_None;
	FName MeshSocket_2 = NAME_None;
	FName SocketOrBoneName;
	FMnhShapeData ShapeData;
	FMnhTraceSettings TraceSettings;
	EMnhTracerTickType TracerTickType;
	float TickInterval = 30;
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;
	bool bUsesTracerConfig = true;
	
	float DeltaTimeLastTick = 0;
	FGuid Guid;
	
	UWorld* World;
	TObjectPtr<UPrimitiveComponent> SourceComponent;
	
	TObjectPtr<UMnhTracer> OwnerTracer;
	int OwnerTracerConfigIdx;
	TObjectPtr<UMnhTracerComponent> OwnerTracerComponent;
	EMnhTracerState TracerState = EMnhTracerState::Stopped;

	FCollisionQueryParams CollisionParams;
	FCollisionObjectQueryParams ObjectQueryParams;
	
	TArray<FMnhMultiTraceResultContainer> SubstepHits;
	TArray<FTransform, TFixedAllocator<2>> TracerTransformsOverTime;
	bool bShouldTickThisFrame = false;
	bool IsPendingRemoval = false;

	
	void ChangeTracerState(bool bIsTracerActiveArg, bool bStopImmediate=true);
	void DoTrace(const uint32 Substeps, const uint32 TickIdx);
	
	FORCEINLINE FTransform GetCurrentTracerTransform()
	{
		FTransform CurrentTransform;
		if (TraceSource == EMnhTraceSource::PhysicsAsset || TraceSource == EMnhTraceSource::AnimNotify)
		{
			const USkeletalMeshComponent* Source = Cast<USkeletalMeshComponent>(SourceComponent);
			if(!Source)
			{
				const auto EnumText = UEnum::GetDisplayValueAsText(TraceSource).ToString();
				const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: Selected Source type is [%s] "
												 "but supplied source component [%s] is not SkeletalMesh"), *EnumText, *SourceComponent.GetName());
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Message);
				Source = nullptr;
				return CurrentTransform;
			}
			const auto BoneTransform = FMnhHelpers::FindBoneTransform(Source, SocketOrBoneName);
			CurrentTransform = FTransform(ShapeData.Orientation, ShapeData.Offset) * BoneTransform;
			CurrentTransform.SetScale3D(FVector::OneVector);
		}
		else if (TraceSource == EMnhTraceSource::MnhShapeComponent || TraceSource == EMnhTraceSource::StaticMeshSockets)
		{
			const auto ComponentCurrentTransform = SourceComponent->GetComponentTransform();
			CurrentTransform = FTransform(ShapeData.Orientation, ShapeData.Offset) * ComponentCurrentTransform;
			CurrentTransform.SetScale3D(FVector::OneVector);
		}
		else if (TraceSource == EMnhTraceSource::SkeletalMeshSockets)
		{
			const USkeletalMeshComponent* Source = Cast<USkeletalMeshComponent>(SourceComponent);
			if(!Source)
			{
				const auto EnumText = UEnum::GetDisplayValueAsText(TraceSource).ToString();
				const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: Selected Source type is [%s] "
												 "but supplied source component [%s] is not SkeletalMesh"), *EnumText, *SourceComponent.GetName());
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Message);
				Source = nullptr;
				return CurrentTransform;
			}
			const auto LengthOffset = ShapeData.HalfSize.X;
			const auto Socket1Transform = Source->GetSocketTransform(MeshSocket_1, RTS_World);
			const auto Socket2Transform = Source->GetSocketTransform(MeshSocket_2, RTS_World);
			const auto NewShapeData =
				FMnhHelpers::GetCapsuleShapeDataFromTransforms(Socket1Transform, Socket2Transform, LengthOffset, ShapeData.Radius);
			ShapeData = NewShapeData;
			ShapeData.HalfSize.X = LengthOffset;
			CurrentTransform = FTransform(ShapeData.Orientation, ShapeData.Offset);
			CurrentTransform.SetScale3D({ShapeData.HalfHeight, 1, 1});
			ShapeData.HalfHeight = 1;
		}
		return CurrentTransform;
	}
	
	FORCEINLINE void UpdatePreviousTransform(const FTransform& Transform)
	{
		if (TracerTransformsOverTime.Num() == 0)
		{
			TracerTransformsOverTime.Add(Transform);
		}
		else
		{
			TracerTransformsOverTime[0] = Transform;
		}
	}
};
