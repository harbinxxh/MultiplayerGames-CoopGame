﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"

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
}


void ASWeapon::Fire()
{
	// Trace the world, from pawn eyes to crosshair location

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

		// Paticle "Target" parameter
		FVector TraceEndPoint = TraceEnd;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, ECollisionChannel::ECC_Visibility, QueryParams))
		{
			// Blocking hit! Process damage

			AActor* HitActor = Hit.GetActor();

			/** Hurts the specified actor with the specified impact.
			* @param DamagedActor - Actor that will be damaged.
			* @param BaseDamage - The base damage to apply.
			* @param HitFromDirection - Direction the hit came FROM
			* @param HitInfo - Collision or trace result that describes the hit
			* @param EventInstigator - Controller that was responsible for causing this damage (e.g. player who shot the weapon)
			* @param DamageCauser - Actor that actually caused the damage (e.g. the grenade that exploded)
			* @param DamageTypeClass - Class that describes the damage that was done.
			* @return Actual damage the ended up being applied to the actor.
			*/
			UGameplayStatics::ApplyPointDamage(HitActor, 20.f, ShotDirection, Hit, MyOwner->GetInstigatorController(), this, DamageType);

			// 击中模型的特效
			if (ImpactEffect)
			{
				/** Plays the specified effect at the given location and rotation, fire and forget. The system will go away when the effect is complete. Does not replicate.
				* @param WorldContextObject - Object that we can obtain a world context from
				* @param EmitterTemplate - particle system to create
				* @param Location - location to place the effect in world space
				* @param Rotation - rotation to place the effect in world space
				* @param Scale - scale to create the effect at
				* @param bAutoDestroy - Whether the component will automatically be destroyed when the particle system completes playing or whether it can be reactivated
				* @param PoolingMethod - Method used for pooling this component. Defaults to none.
				*/
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
			}

			TraceEndPoint = Hit.ImpactPoint;
		}

		if (DebugWeaponDrawing > 0)
		{
			DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::White, false, 1.0f, 0, 1.0f);
		}

		PlayFireEffects(TraceEndPoint);
	}
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

