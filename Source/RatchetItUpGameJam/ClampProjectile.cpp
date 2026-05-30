// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClampProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "CableComponent.h"
#include "GameFramework/Pawn.h"

AClampProjectile::AClampProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// The mesh doubles as the collision used for flight and hit detection, and is the root.
	ClampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClampMesh"));
	ClampMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ClampMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ClampMesh->SetSimulatePhysics(false);
	ClampMesh->SetNotifyRigidBodyCollision(true);
	RootComponent = ClampMesh;

	// Flies the clamp forward in a straight line until it stops on a blocking hit.
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(ClampMesh);
	ProjectileMovement->InitialSpeed = 0.f;   // set in Launch()
	ProjectileMovement->MaxSpeed = 0.f;        // set in Launch()
	ProjectileMovement->bShouldBounce = false; // stop on first hit so it latches
	ProjectileMovement->ProjectileGravityScale = 0.f; // straight shot, no drop
	ProjectileMovement->bAutoActivate = false;

	// The wire. The END is glued to the clamp (set in BeginPlay); the START is anchored to the
	// character when AttachCableStartToCharacter() is called.
	Cable = CreateDefaultSubobject<UCableComponent>(TEXT("Cable"));
	Cable->SetupAttachment(ClampMesh);
	Cable->bAttachStart = true;  // start follows whatever the cable component is parented to
	Cable->bAttachEnd = true;    // end is glued to the component we attach it to (the clamp)
	Cable->CableLength = 0.f;    // let it stretch between the two endpoints
	Cable->NumSegments = 10;
	Cable->CableWidth = 4.f;
	Cable->SolverIterations = 4;
}

void AClampProjectile::BeginPlay()
{
	Super::BeginPlay();

	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AClampProjectile::OnProjectileStop);

	// Glue the END of the wire to the clamp body. The START is anchored to the character later
	// via AttachCableStartToCharacter().
	Cable->SetAttachEndToComponent(ClampMesh, NAME_None);
}

void AClampProjectile::AttachCableStartToCharacter(USceneComponent* AttachComponent, FName SocketName)
{
	if (!AttachComponent)
	{
		return;
	}

	// Re-parent the cable so its START point follows the character (e.g. a hand socket).
	Cable->AttachToComponent(AttachComponent, FAttachmentTransformRules::KeepWorldTransform, SocketName);
	Cable->bAttachStart = true;
}

void AClampProjectile::Launch(APawn* InOwner, FVector Direction)
{
	OwningPawn = InOwner;
	LaunchOrigin = GetActorLocation();

	// Don't collide with / stop on the player that fired us.
	if (OwningPawn)
	{
		ClampMesh->IgnoreActorWhenMoving(OwningPawn, true);
	}

	// Point the clamp the way it's flying and fire it.
	const FVector ShotDir = Direction.GetSafeNormal();
	if (!ShotDir.IsNearlyZero())
	{
		SetActorRotation(ShotDir.Rotation());
	}

	ProjectileMovement->MaxSpeed = LaunchSpeed;
	ProjectileMovement->Velocity = ShotDir * LaunchSpeed;
	ProjectileMovement->Activate();
}

void AClampProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	if (bAttached)
	{
		return;
	}

	bAttached = true;

	// Freeze the clamp where it landed.
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();

	// Stick to whatever we hit so the clamp rides along if that thing moves.
	if (USceneComponent* HitComp = ImpactResult.GetComponent())
	{
		AttachToComponent(HitComp, FAttachmentTransformRules::KeepWorldTransform);
	}

	// Capture the tether length if it wasn't authored explicitly.
	if (RopeLength <= 0.f && OwningPawn)
	{
		RopeLength = FVector::Dist(GetActorLocation(), OwningPawn->GetActorLocation());
	}

	OnClampAttached.Broadcast(ImpactResult.GetActor());
}

void AClampProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAttached)
	{
		// Give up and report a miss once we've flown the full range.
		if (FVector::DistSquared(LaunchOrigin, GetActorLocation()) >= FMath::Square(MaxRange))
		{
			OnClampMissed.Broadcast();
			Release();
		}
		return;
	}

	// Hard tether: keep the player within rope length of the clamp without ever pulling them in.
	if (bLimitPlayerToRopeLength && OwningPawn && RopeLength > 0.f)
	{
		const FVector ClampLoc = GetActorLocation();
		const FVector PlayerLoc = OwningPawn->GetActorLocation();
		const FVector ToPlayer = PlayerLoc - ClampLoc;

		if (ToPlayer.SizeSquared() > FMath::Square(RopeLength))
		{
			const FVector Constrained = ClampLoc + ToPlayer.GetSafeNormal() * RopeLength;
			OwningPawn->SetActorLocation(Constrained, true);
		}
	}
}

void AClampProjectile::Release()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Destroy();
}
