// Fill out your copyright notice in the Description page of Project Settings.

#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CoopGame.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"


static int32 DebugWeaponDrawing = 0;
FAutoConsoleVariableRef CVARDebugWeaponDrawing(
	TEXT("COOP.DebugWeapons"),
	DebugWeaponDrawing,
	TEXT("Draw Debug Lines for Weapons"),
	ECVF_Cheat);

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "Target";

	BaseDamage = 20.f;

	RateOfFire = 600;

	SetReplicates(true);

	NetUpdateFrequency = 66.0f;		// How often (per second) this actor will be considered for replication, used to determine NetUpdateTime
	MinNetUpdateFrequency = 33.0f;	// Used to determine what rate to throttle down to when replicated properties are changing infrequently
}

void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetwwenShots = 60 / RateOfFire;
}

void ASWeapon::Fire()
{
	// Trace the world, from pawn eyes to crosshair location

	if (Role < ROLE_Authority) // Client调用服务方法
	{
		ServerFire();
	}

	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		FVector ShotDirection = EyeRotation.Vector();

		FVector TraceEnd = EyeLocation + (ShotDirection * 10000);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(MyOwner);
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;
		QueryParams.bReturnPhysicalMaterial = true;

		// Paticle "Target" parameter
		FVector TraceEndPoint = TraceEnd;

		EPhysicalSurface SurfaceType = EPhysicalSurface::SurfaceType_Default;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_WEAPON, QueryParams))
		{
			// Blocking hit! Process damage
			AActor* HitActor = Hit.GetActor();

			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

			float ActualDamage = BaseDamage;
			if (SurfaceType == SURFACE_FLESHVULNERABLE) // 通过Physical Surface来设置不同的伤害
			{
				ActualDamage *= 4.0f;
			}

			UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, MyOwner->GetInstigatorController(), this, DamageType);
			
			PlayImpactEffects(SurfaceType, Hit.ImpactPoint);

			TraceEndPoint = Hit.ImpactPoint;
		}

		if (DebugWeaponDrawing > 0)
		{
			DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::White, false, 1.0f, 0, 1.0f);
		}

		PlayFireEffects(TraceEndPoint);
		
		// 最重要的一件事， 当在服务器射击时，需要更新 FHitScanTrace 结构变量
		if (Role == ROLE_Authority)
		{
			HitScanTrace.TraceTo = TraceEndPoint;
			HitScanTrace.SurfaceType = SurfaceType;
		}

		LastFireTime = GetWorld()->TimeSeconds;
	}
}

void ASWeapon::OnRep_HitScanTrace()
{
	// This whole struct gets replicated to all the clients we replace some fire effects.
	// Play Cosmetic FX
	PlayFireEffects(HitScanTrace.TraceTo);

	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceTo);
}

void ASWeapon::ServerFire_Implementation()
{
	Fire();
}

bool ASWeapon::ServerFire_Validate()
{
	return true;
}

void ASWeapon::StartFire()
{
	float FirstDelay = FMath::Max(LastFireTime + TimeBetwwenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimerHandle_TimeBetweenShot, this, &ASWeapon::Fire, TimeBetwwenShots, true, FirstDelay);
}

void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBetweenShot);
}

void ASWeapon::PlayFireEffects(FVector TraceEnd)
{
	// 枪口火花特效
	if (MuzzleEffect)
	{
		/** Plays the specified effect attached to and following the specified component. The system will go away when the effect is complete. Does not replicate.
		* @param EmitterTemplate - particle system to create
		* @param AttachComponent - Component to attach to.
		* @param AttachPointName - Optional named point within the AttachComponent to spawn the emitter at
		* @param Location - Depending on the value of LocationType this is either a relative offset from the attach component/point or an absolute world location that will be translated to a relative offset (if LocationType is KeepWorldPosition).
		* @param Rotation - Depending on the value of LocationType this is either a relative offset from the attach component/point or an absolute world rotation that will be translated to a relative offset (if LocationType is KeepWorldPosition).
		* @param Scale - Depending on the value of LocationType this is either a relative scale from the attach component or an absolute world scale that will be translated to a relative scale (if LocationType is KeepWorldPosition).
		* @param LocationType - Specifies whether Location is a relative offset or an absolute world position
		* @param bAutoDestroy - Whether the component will automatically be destroyed when the particle system completes playing or whether it can be reactivated
		* @param PoolingMethod - Method used for pooling this component. Defaults to none.
		*/
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}

	// 横梁烟雾效果(Smoke Beam Effect)
	if (TracerEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);
		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);
		if (TracerComp)
		{
			TracerComp->SetVectorParameter(TracerTargetName, TraceEnd);
		}
	}

	// Play Camera Shake 
	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{
		APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
		if (PC)
		{
			PC->ClientPlayCameraShake(FireCamShake);
		}
	}
}

void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	UParticleSystem* SelectedEffect = nullptr;
	switch (SurfaceType)
	{
	case SURFACE_FLESHDEFAULT:
	case SURFACE_FLESHVULNERABLE:
		SelectedEffect = FleshImpactEffect;
		break;
	default:
		SelectedEffect = DefaultImpactEffect;
		break;
	}

	if (SelectedEffect) // 击中模型的特效
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);
		FVector ShotDirction = ImpactPoint - MuzzleLocation;
		ShotDirction.Normalize();

		// 在指定的位置播放指定的效果，并旋转、射击和遗忘。当效果完成时，系统就会消失。不复制。
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, ImpactPoint, ShotDirction.Rotation());
	}
}

void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 条件说明： Because I would not want to replicate this to the clients that owns this weapon.
	// 所以我不需要把它发送到服务器然后再通过服务器接收它
	// 因为我们已经执行了代码所以它会运行两次视觉效果
	DOREPLIFETIME_CONDITION(ASWeapon, HitScanTrace, COND_SkipOwner);
}
