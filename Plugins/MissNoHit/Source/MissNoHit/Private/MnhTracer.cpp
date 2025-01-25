// Copyright 2024 Eren Balatkan. All Rights Reserved.

#include "MnhTracer.h"

#include "MissNoHit.h"
#include "DrawDebugHelpers.h"
#include "MnhComponents.h"
#include "MnhHelpers.h"
#include "MnhTracerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsEngine/BodySetup.h"

class FMissNoHitModule;

UMnhTracer::~UMnhTracer()
{
	MarkTracerDataForRemoval();
}


void UMnhTracer::InitializeParameters(UPrimitiveComponent* SourceComponentArg)
{
	if (!SourceComponentArg)
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: Source Component is null!"));
		FMnhHelpers::Mnh_Log(Message);
		return;
	}
	
	this->SourceComponent = SourceComponentArg;

	CollisionParams.AddIgnoredActor(SourceComponent->GetOwner());
	CollisionParams.bReturnPhysicalMaterial = true;
	CollisionParams.bTraceComplex = TraceSettings.bTraceComplex;
	
	for (const auto& ObjectType : TraceSettings.ObjectTypes)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ObjectType);
	}
	switch (TraceSource)
	{
	case EMnhTraceSource::PhysicsAsset:
		InitializeFromPhysicAsset();
		break;
	case EMnhTraceSource::MnhShapeComponent:
		InitializeFromCollisionShape();
		break;
	case EMnhTraceSource::StaticMeshSockets:
		InitializeFromStaticMeshSockets();
		break;
	case EMnhTraceSource::SkeletalMeshSockets:
		InitializeFromSkeletalMeshSockets();
		break;
	default:
		break;
	}
	
	UpdateTracerData();
}

void UMnhTracer::InitializeFromPhysicAsset()
{
	const USkeletalMeshComponent* Source = Cast<USkeletalMeshComponent>(SourceComponent);
	if(!Source)
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: Selected Source type is PhysicAsset but supplied source component type is not SkeletalMesh: %s"), *SourceComponent.GetName());
		FMnhHelpers::Mnh_Log(Message);
		SourceComponent = nullptr;
		return;
	}

	const auto BodyInstance = Source->GetBodyInstance(SocketOrBoneName);
	if (!BodyInstance)
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: No BodyInstance found with Bone Name [%s] on Component [%s]. "
									  "Please use Mnh Shape Component Attached to Bone Instead"), *SocketOrBoneName.ToString(), *SourceComponent.GetName());
		FMnhHelpers::Mnh_Log(Message);
		
		SourceComponent = nullptr;
		return;
	}
	
	const auto Geom = BodyInstance->GetBodySetup()->AggGeom;
	for(const auto& Sphere : Geom.SphereElems)
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Sphere;
		this->ShapeData.Offset = Sphere.Center;
		this->ShapeData.Radius = Sphere.Radius;
		return;
	}
	for (const auto& Box : Geom.BoxElems)
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Box;
		this->ShapeData.Offset = Box.Center;
		this->ShapeData.HalfSize = {Box.X/2, Box.Y/2, Box.Z/2};
		this->ShapeData.Orientation = Box.Rotation;
		return;
	}
	for (const auto& Capsule : Geom.SphylElems)
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Capsule;
		this->ShapeData.Offset = Capsule.Center;
		this->ShapeData.HalfHeight = Capsule.GetScaledHalfLength(FVector::OneVector);
		this->ShapeData.Orientation = Capsule.Rotation;
		return;
	}
	const auto Message = FString::Printf(TEXT("MissNoHit Warning: No compatible geometry was found with Bone Name [%s] on Component [%s]"), *SocketOrBoneName.ToString(), *SourceComponent.GetName());
	FMnhHelpers::Mnh_Log(Message);
}

void UMnhTracer::InitializeFromCollisionShape()
{
	if (const auto SphereCollision = Cast<UMnhSphereComponent>(SourceComponent))
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Sphere;
		this->ShapeData.Radius = SphereCollision->GetScaledSphereRadius();
		return;
	}
	else if (const auto BoxCollision = Cast<UMnhBoxComponent>(SourceComponent))
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Box;
		this->ShapeData.HalfSize = BoxCollision->GetScaledBoxExtent();
		return;
	}
	else if (const auto CapsuleCollision = Cast<UMnhCapsuleComponent>(SourceComponent))
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Capsule;
		this->ShapeData.HalfHeight = CapsuleCollision->GetScaledCapsuleHalfHeight();
		this->ShapeData.Radius = CapsuleCollision->GetScaledCapsuleRadius();
		return;
	}
	const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization of Tracer Source unsuccessful! "
										   "Mnh Component Tracer expects Sphere, Box or Capsule component as input, not [%s] on Actor [%s]"),
										   *SourceComponent.GetName(),
										   *SourceComponent->GetOuter()->GetName());
	this->SourceComponent = nullptr;
	FMnhHelpers::Mnh_Log(Message);
}

void UMnhTracer::InitializeFromStaticMeshSockets()
{
	if (const auto StaticMeshSource = Cast<UStaticMeshComponent>(SourceComponent))
	{
		const auto Socket1Transform = StaticMeshSource->GetSocketTransform(MeshSocket_1, RTS_Component);
		const auto Socket2Transform = StaticMeshSource->GetSocketTransform(MeshSocket_2, RTS_Component);
		this->ShapeData = FMnhHelpers::GetCapsuleShapeDataFromTransforms(Socket1Transform, Socket2Transform, MeshSocketTracerLengthOffset, MeshSocketTracerRadius);
	}
	else
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization of Tracer Source unsuccessful! "
										   "Supplied Source is not Static Mesh Component [%s] on Actor [%s]"),
										   *SourceComponent.GetName(),
										   *SourceComponent->GetOuter()->GetName());
		FMnhHelpers::Mnh_Log(Message);
		SourceComponent = nullptr;
	}
}

void UMnhTracer::InitializeFromSkeletalMeshSockets()
{
	if(const auto SkeletalMeshSource= Cast<USkeletalMeshComponent>(SourceComponent))
	{
		const auto Socket1Transform = SkeletalMeshSource->GetSocketTransform(MeshSocket_1, RTS_Component);
		const auto Socket2Transform = SkeletalMeshSource->GetSocketTransform(MeshSocket_2, RTS_Component);
		this->ShapeData = FMnhHelpers::GetCapsuleShapeDataFromTransforms(
			Socket1Transform, Socket2Transform, MeshSocketTracerLengthOffset, MeshSocketTracerRadius);
		this->ShapeData.HalfSize.X = MeshSocketTracerLengthOffset;
	}
	else
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization of Tracer Source unsuccessful! "
										   "Supplied Source is not Skeletal Mesh Component [%s] on Actor [%s]"),
										   *SourceComponent.GetName(),
										   *SourceComponent->GetOuter()->GetName());
		FMnhHelpers::Mnh_Log(Message);
		SourceComponent = nullptr;
	}
}

bool UMnhTracer::IsTracerActive() const
{
	return GetTracerData().TracerState != EMnhTracerState::Stopped;
}

void UMnhTracer::ChangeTracerState(const bool bIsTracerActiveArg, const bool bStopImmediate)
{
	bIsTracerActive = bIsTracerActiveArg;
	GetTracerData().ChangeTracerState(bIsTracerActiveArg, bStopImmediate);
}

void UMnhTracer::RegisterTracerData()
{
	SCOPE_CYCLE_COUNTER(STAT_MnhAddTracer)
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	TracerDataIdx = MnhModule.RequestNewTracerData();
	TracerDataGuid = FGuid::NewGuid();
	UpdateTracerData();
}

void UMnhTracer::UpdateTracerData()
{
	float TickInterval = 0;
	if (TracerTickType == EMnhTracerTickType::DistanceTick)
	{
		TickInterval = TickDistanceTraveled;
	}
	else if (TracerTickType == EMnhTracerTickType::FixedRateTick)
	{
		TickInterval = 1.0f/float(TargetFps);
	}
	
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	auto& TracerData = MnhModule.GetTracerDataAt(TracerDataIdx);
	TracerData.OwnerTracer = this;
	TracerData.Guid = TracerDataGuid;
	TracerData.TraceSource = TraceSource;
	TracerData.ShapeData = ShapeData;
	TracerData.SocketOrBoneName = SocketOrBoneName;
	TracerData.MeshSocket_1 = MeshSocket_1;
	TracerData.MeshSocket_2 = MeshSocket_2;
	TracerData.TraceSettings = TraceSettings;
	TracerData.SourceComponent = SourceComponent;
	TracerData.OwnerTracerComponent = OwnerComponent;
	TracerData.TracerState = EMnhTracerState::Stopped;
	TracerData.DeltaTimeLastTick = 0;
	TracerData.TracerTickType = TracerTickType;
	TracerData.TickInterval = TickInterval;
	TracerData.bShouldTickThisFrame = false;
	TracerData.CollisionParams = CollisionParams;
	TracerData.ObjectQueryParams = ObjectQueryParams;
	TracerData.bUsesTracerConfig = false;
	TracerData.World = GetWorld();
}

void UMnhTracer::MarkTracerDataForRemoval() const
{
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	MnhModule.MarkTracerDataForRemoval(TracerDataIdx, TracerDataGuid);
}

FMnhTracerData& UMnhTracer::GetTracerData() const
{
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	auto& TracerData = MnhModule.GetTracerDataAt(TracerDataIdx);
	if (TracerData.Guid != TracerDataGuid)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, "Retrieved TracerData Guid does not match Tracer Guid!");
	}
	return TracerData;
}

void FMnhTracerData::ChangeTracerState(const bool bIsTracerActiveArg, const bool bStopImmediate)
{
	if (bIsTracerActiveArg)
	{
		this->TracerState = EMnhTracerState::Active;
		this->DeltaTimeLastTick = 0;
		if (SourceComponent)
		{
			UpdatePreviousTransform(GetCurrentTracerTransform());
		}
	}
	else
	{
		if (this->TracerState == EMnhTracerState::Active && !bStopImmediate)
		{
			this->TracerState = EMnhTracerState::PendingStop;
		}
		else
		{
			this->TracerState = EMnhTracerState::Stopped;
		}
	}
}

void FMnhTracerData::DoTrace(const uint32 Substeps, const uint32 TickIdx)
{
	SubstepHits.Reset();
	if (TracerTransformsOverTime.Num() > 1)
	{
		const float SubstepRatio = 1.0 / Substeps;
		const FTransform CurrentTransform = TracerTransformsOverTime.Last();
		const FTransform PreviousTransform = TracerTransformsOverTime[0];

		for (uint32 i = 0; i<Substeps; i++)
		{
			// Respect cancellations by user-defined code immediately.
			if (TracerState == EMnhTracerState::Stopped)
			{
				break;
			}
			
			const FTransform& StartTransform = UKismetMathLibrary::TLerp(PreviousTransform, CurrentTransform, SubstepRatio * i, ELerpInterpolationMode::DualQuatInterp);
			const FTransform& EndTransform = UKismetMathLibrary::TLerp(PreviousTransform, CurrentTransform, SubstepRatio * (i+1), ELerpInterpolationMode::DualQuatInterp);
			const auto& AverageTransform = UKismetMathLibrary::TLerp(StartTransform, EndTransform, 0.5, ELerpInterpolationMode::DualQuatInterp);
			
			TArray<FHitResult> OutHits;
			FMnhHelpers::PerformTrace(StartTransform, EndTransform, AverageTransform,
				OutHits, World, TraceSettings, ShapeData, CollisionParams, FCollisionResponseParams(), ObjectQueryParams);
			
			SubstepHits.Add(
				{
					StartTransform.GetLocation(),
					EndTransform.GetLocation(),
					AverageTransform.GetScale3D(),
					AverageTransform.GetRotation(),
					OutHits
				});
		}
	}
	else
	{
		const FString& DebugMessage = FString::Printf(TEXT("MissNoHit Warning: Tracer [%s] has less than 2 transforms in TracerTransformsOverTime array!"), *OwnerTracer->TracerTag.ToString());
		FMnhHelpers::Mnh_Log(DebugMessage);
	}
}



void FMnhTracerConfig::InitializeParameters(UPrimitiveComponent* SourceComponentArg)
{
	if (!SourceComponentArg)
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: Source Component is null!"));
		FMnhHelpers::Mnh_Log(Message);
		return;
	}
	
	this->SourceComponent = SourceComponentArg;

	CollisionParams.AddIgnoredActor(SourceComponent->GetOwner());
	CollisionParams.AddIgnoredActor(OwnerTracerComponent->GetOwner());
	CollisionParams.bReturnPhysicalMaterial = true;
	CollisionParams.bTraceComplex = TraceSettings.bTraceComplex;
	
	for (const auto& ObjectType : TraceSettings.ObjectTypes)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ObjectType);
	}
	switch (TraceSource)
	{
	case EMnhTraceSource::PhysicsAsset:
		InitializeFromPhysicAsset();
		break;
	case EMnhTraceSource::MnhShapeComponent:
		InitializeFromCollisionShape();
		break;
	case EMnhTraceSource::StaticMeshSockets:
		InitializeFromStaticMeshSockets();
		break;
	case EMnhTraceSource::SkeletalMeshSockets:
		InitializeFromSkeletalMeshSockets();
		break;
	default:
		break;
	}
	
	UpdateTracerData();
}

void FMnhTracerConfig::InitializeFromPhysicAsset()
{
	const USkeletalMeshComponent* Source = Cast<USkeletalMeshComponent>(SourceComponent);
	if(!Source)
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: Selected Source type is PhysicAsset but supplied source component type is not SkeletalMesh: %s"), *SourceComponent.GetName());
		FMnhHelpers::Mnh_Log(Message);
		SourceComponent = nullptr;
		return;
	}

	const auto BodyInstance = Source->GetBodyInstance(SocketOrBoneName);
	if (!BodyInstance)
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization Error: No BodyInstance found with Bone Name [%s] on Component [%s]. "
									  "Please use Mnh Shape Component Attached to Bone Instead"), *SocketOrBoneName.ToString(), *SourceComponent.GetName());
		FMnhHelpers::Mnh_Log(Message);
		
		SourceComponent = nullptr;
		return;
	}
	
	const auto Geom = BodyInstance->GetBodySetup()->AggGeom;
	for(const auto& Sphere : Geom.SphereElems)
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Sphere;
		this->ShapeData.Offset = Sphere.Center;
		this->ShapeData.Radius = Sphere.Radius;
		return;
	}
	for (const auto& Box : Geom.BoxElems)
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Box;
		this->ShapeData.Offset = Box.Center;
		this->ShapeData.HalfSize = {Box.X/2, Box.Y/2, Box.Z/2};
		this->ShapeData.Orientation = Box.Rotation;
		return;
	}
	for (const auto& Capsule : Geom.SphylElems)
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Capsule;
		this->ShapeData.Offset = Capsule.Center;
		this->ShapeData.HalfHeight = Capsule.GetScaledHalfLength(FVector::OneVector);
		this->ShapeData.Orientation = Capsule.Rotation;
		return;
	}
	const auto Message = FString::Printf(TEXT("MissNoHit Warning: No compatible geometry was found with Bone Name [%s] on Component [%s]"), *SocketOrBoneName.ToString(), *SourceComponent.GetName());
	FMnhHelpers::Mnh_Log(Message);
}

void FMnhTracerConfig::InitializeFromCollisionShape()
{
	if (const auto SphereCollision = Cast<UMnhSphereComponent>(SourceComponent))
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Sphere;
		this->ShapeData.Radius = SphereCollision->GetScaledSphereRadius();
		return;
	}
	else if (const auto BoxCollision = Cast<UMnhBoxComponent>(SourceComponent))
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Box;
		this->ShapeData.HalfSize = BoxCollision->GetScaledBoxExtent();
		return;
	}
	else if (const auto CapsuleCollision = Cast<UMnhCapsuleComponent>(SourceComponent))
	{
		this->ShapeData.TraceShape = EMnhTraceShape::Capsule;
		this->ShapeData.HalfHeight = CapsuleCollision->GetScaledCapsuleHalfHeight();
		this->ShapeData.Radius = CapsuleCollision->GetScaledCapsuleRadius();
		return;
	}
	const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization of Tracer Source unsuccessful! "
										   "Mnh Component Tracer expects Sphere, Box or Capsule component as input, not [%s] on Actor [%s]"),
										   *SourceComponent.GetName(),
										   *SourceComponent->GetOuter()->GetName());
	this->SourceComponent = nullptr;
	FMnhHelpers::Mnh_Log(Message);
}

void FMnhTracerConfig::InitializeFromStaticMeshSockets()
{
	if (const auto StaticMeshSource = Cast<UStaticMeshComponent>(SourceComponent))
	{
		const auto Socket1Transform = StaticMeshSource->GetSocketTransform(MeshSocket_1, RTS_Component);
		const auto Socket2Transform = StaticMeshSource->GetSocketTransform(MeshSocket_2, RTS_Component);
		this->ShapeData = FMnhHelpers::GetCapsuleShapeDataFromTransforms(Socket1Transform, Socket2Transform, MeshSocketTracerLengthOffset, MeshSocketTracerRadius);
	}
	else
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization of Tracer Source unsuccessful! "
										   "Supplied Source is not Static Mesh Component [%s] on Actor [%s]"),
										   *SourceComponent.GetName(),
										   *SourceComponent->GetOuter()->GetName());
		FMnhHelpers::Mnh_Log(Message);
		SourceComponent = nullptr;
	}
}

void FMnhTracerConfig::InitializeFromSkeletalMeshSockets()
{
	if(const auto SkeletalMeshSource= Cast<USkeletalMeshComponent>(SourceComponent))
	{
		const auto Socket1Transform = SkeletalMeshSource->GetSocketTransform(MeshSocket_1, RTS_Component);
		const auto Socket2Transform = SkeletalMeshSource->GetSocketTransform(MeshSocket_2, RTS_Component);
		this->ShapeData = FMnhHelpers::GetCapsuleShapeDataFromTransforms(
			Socket1Transform, Socket2Transform, MeshSocketTracerLengthOffset, MeshSocketTracerRadius);
		this->ShapeData.HalfSize.X = MeshSocketTracerLengthOffset;
	}
	else
	{
		const auto Message = FString::Printf(TEXT("MissNoHit Warning: Initialization of Tracer Source unsuccessful! "
										   "Supplied Source is not Skeletal Mesh Component [%s] on Actor [%s]"),
										   *SourceComponent.GetName(),
										   *SourceComponent->GetOuter()->GetName());
		FMnhHelpers::Mnh_Log(Message);
		SourceComponent = nullptr;
	}
}

void FMnhTracerConfig::ChangeTracerState(bool bIsTracerActiveArg, bool bStopImmediate)
{
	bIsTracerActive = bIsTracerActiveArg;
	GetTracerData().ChangeTracerState(bIsTracerActiveArg, bStopImmediate);
}

bool FMnhTracerConfig::IsTracerActive() const
{
	return GetTracerData().TracerState != EMnhTracerState::Stopped;
}

void FMnhTracerConfig::RegisterTracerData()
{
	SCOPE_CYCLE_COUNTER(STAT_MnhAddTracer)
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	TracerDataIdx = MnhModule.RequestNewTracerData();
	TracerDataGuid = FGuid::NewGuid();
	UpdateTracerData();
}

void FMnhTracerConfig::UpdateTracerData() const
{
	float TickInterval = 0;
	if (TracerTickType == EMnhTracerTickType::DistanceTick)
	{
		TickInterval = TickDistanceTraveled;
	}
	else if (TracerTickType == EMnhTracerTickType::FixedRateTick)
	{
		TickInterval = 1.0f/float(TargetFps);
	}
	
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	auto& TracerData = MnhModule.GetTracerDataAt(TracerDataIdx);
	TracerData.OwnerTracerConfigIdx = OwnerTracerConfigIdx;
	TracerData.Guid = TracerDataGuid;
	TracerData.TraceSource = TraceSource;
	TracerData.ShapeData = ShapeData;
	TracerData.SocketOrBoneName = SocketOrBoneName;
	TracerData.MeshSocket_1 = MeshSocket_1;
	TracerData.MeshSocket_2 = MeshSocket_2;
	TracerData.TraceSettings = TraceSettings;
	TracerData.SourceComponent = SourceComponent;
	TracerData.OwnerTracerComponent = OwnerTracerComponent;
	TracerData.DeltaTimeLastTick = 0;
	TracerData.TracerTickType = TracerTickType;
	TracerData.TickInterval = TickInterval;
	TracerData.bShouldTickThisFrame = false;
	TracerData.TracerState = EMnhTracerState::Stopped;
	TracerData.CollisionParams = CollisionParams;
	TracerData.ObjectQueryParams = ObjectQueryParams;
	TracerData.World = OwnerTracerComponent->GetOwner()->GetWorld();
}

void FMnhTracerConfig::MarkTracerDataForRemoval() const
{
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	MnhModule.MarkTracerDataForRemoval(TracerDataIdx, TracerDataGuid);
}

FMnhTracerData& FMnhTracerConfig::GetTracerData() const
{
	FMissNoHitModule& MnhModule = FModuleManager::LoadModuleChecked<FMissNoHitModule>("MissNoHit");
	auto& TracerData = MnhModule.GetTracerDataAt(TracerDataIdx);
	if (TracerData.Guid != TracerDataGuid)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, "Retrieved TracerData Guid does not match Tracer Guid!");
	}
	return TracerData;
}