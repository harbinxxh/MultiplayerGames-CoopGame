// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPowerupActor.generated.h"

UCLASS()
class COOPGAME_API ASPowerupActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASPowerupActor();

protected:

	/* Time between powerup ticks */
	UPROPERTY(EditDefaultsOnly, Category = "Powerups")
	float PowerupInterval;

	/* Total times we apply the powerup effect */
	UPROPERTY(EditDefaultsOnly, Category = "Powerups")
	float TotalNrOfTicks;

	FTimerHandle TimerHandle_PowerupTick;

	// Total number of ticks applied
	int32 TicksProcessed;

	UFUNCTION()
	void OnTickPowerup();

	// Keep state of the power-up
	UPROPERTY(ReplicatedUsing=OnRep_PowerupActive)
	bool bIsPowerupActive;

	UFUNCTION()
	void OnRep_PowerupActive();

	UFUNCTION(BlueprintImplementableEvent, Category = "Powerups")
	void OnPowerupStateChanged(bool bNewIsActive);

public:

	// 这个函数将会在我们 Pickup 的时候被调用而不是在生成的时候
	void ActivatePowerup();

	UFUNCTION(BlueprintImplementableEvent, Category = "Powerups")
	void OnActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Powerups")
	void OnPowerupTicked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Powerups")
	void OnExpired();
	
};
