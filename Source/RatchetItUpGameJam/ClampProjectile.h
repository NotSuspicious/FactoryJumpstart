// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClampProjectile.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;
class UCableComponent;

/** Fired when the clamp latches onto something. AttachedActor is what it stuck to (may be null for static geometry). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FClampAttachedSignature, AActor*, AttachedActor);

/** Fired if the clamp travels its full range without hitting anything. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FClampMissedSignature);

/**
 *  A clamp that is shot from the player, flies forward, and latches onto the first
 *  thing it touches. A cable (wire) connects the player to the clamp and stays
 *  connected. Optionally the cable acts as a tether that stops the player from
 *  walking farther than the rope's length (no reeling / pulling).
 *
 *  Reparent BP_Clamp to this class in the editor, then assign a mesh to ClampMesh.
 */
UCLASS(abstract)
class AClampProjectile : public AActor
{
	GENERATED_BODY()

public:

	AClampProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  Launches the clamp away from the player.
	 *  @param InOwner    The player pawn that fired the clamp (used as the wire anchor and ignored by collision).
	 *  @param Direction  World-space direction to fire in (does not need to be normalized).
	 */
	UFUNCTION(BlueprintCallable, Category = "Clamp")
	void Launch(APawn* InOwner, FVector Direction);

	/**
	 *  Attaches the START of the wire to the character (call this from the character on itself).
	 *  The END of the wire stays attached to the clamp. Typically called right after spawning the
	 *  clamp; pass a mesh component and an optional socket (e.g. a hand bone) to anchor the wire to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Clamp")
	void AttachCableStartToCharacter(USceneComponent* AttachComponent, FName SocketName);

	/** Detaches the clamp, removes the tether and destroys it (call this to "let go" of the rope). */
	UFUNCTION(BlueprintCallable, Category = "Clamp")
	void Release();

	/** True once the clamp has latched onto something. */
	UFUNCTION(BlueprintPure, Category = "Clamp")
	bool IsAttached() const { return bAttached; }

	/** Broadcast when the clamp latches onto something. */
	UPROPERTY(BlueprintAssignable, Category = "Clamp")
	FClampAttachedSignature OnClampAttached;

	/** Broadcast when the clamp flies its full range without hitting anything. */
	UPROPERTY(BlueprintAssignable, Category = "Clamp")
	FClampMissedSignature OnClampMissed;

protected:

	/** Visible clamp body and the collision used for flight + hit detection. Acts as the root. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ClampMesh;

	/** Drives the clamp forward until it hits something. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** The wire that connects the player to the clamp. Tweak its material / width / segments here in BP_Clamp. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCableComponent> Cable;

	/** Speed (cm/s) the clamp is fired at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp")
	float LaunchSpeed = 4000.f;

	/** How far the clamp can travel before it gives up and reports a miss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp")
	float MaxRange = 3000.f;

	/**
	 *  If true, once attached the player cannot walk farther from the clamp than the rope length
	 *  (a hard tether). The player still moves freely within that radius and is never pulled in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp|Tether")
	bool bLimitPlayerToRopeLength = true;

	/**
	 *  Rope length used for the tether. If <= 0, the length is captured automatically as the
	 *  player-to-clamp distance at the moment it latches on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clamp|Tether")
	float RopeLength = 0.f;

	virtual void BeginPlay() override;

	/** Called by the ProjectileMovement when the clamp stops (i.e. hits a blocking surface). */
	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	/** The pawn that fired this clamp; anchors the wire and is the thing the tether constrains. */
	UPROPERTY(BlueprintReadOnly, Category = "Clamp", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APawn> OwningPawn;

	/** True once latched. */
	bool bAttached = false;

	/** Where the clamp started, used to measure travel distance for the miss check. */
	FVector LaunchOrigin = FVector::ZeroVector;
};
