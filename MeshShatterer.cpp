// Fill out your copyright notice in the Description page of Project Settings.

#include "MeshShatterer.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

// Sets default values for this component's properties
UMeshShatterer::UMeshShatterer()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	//ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	//ProcMesh->SetSimulatePhysics(true);
}


void UMeshShatterer::BeginPlay()
{
	Super::BeginPlay();

	//CreateProceduralMesh();
	ShatterToGround();

	/*ProcMesh->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
	ProcMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	ProcMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ProcMesh->SetEnableGravity(true);
	ProcMesh->WakeRigidBody();
	ProcMesh->SetMobility(EComponentMobility::Movable);*/
}


// Called every frame
void UMeshShatterer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


void UMeshShatterer::CreateProceduralMesh()
{
	UStaticMeshComponent* StaticMesh = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if(StaticMesh != nullptr)
	{
		//Copy vert positions
		const FPositionVertexBuffer& StaticVertPositions = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].PositionVertexBuffer;
		TArray<FVector> ProcVertexBuffer;
		for(uint32 i = 0 ; i < StaticVertPositions.GetNumVertices() ; ++i)
		{
			FVector TransformedPosition = StaticMesh->GetComponentTransform().TransformPosition(StaticVertPositions.VertexPosition(i));
			ProcVertexBuffer.Emplace(TransformedPosition);
		}

		//Copy indices
		const FRawStaticIndexBuffer& StaticIndices = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].IndexBuffer;
		const FIndexArrayView& StaticIndicesBuffer = StaticIndices.GetArrayView();
		TArray<int32> ProcIndicesBuffer;
		ProcIndicesBuffer.Reserve(StaticIndicesBuffer.Num());
		for(int32 i = 0 ; i < StaticIndicesBuffer.Num() ; ++i)
		{
			ProcIndicesBuffer[i] = StaticIndicesBuffer[i];
		}

		//Copy normals, UVs, vert colors, tangents
		const FStaticMeshVertexBuffer& StaticVerts = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].VertexBuffer;
		const FColorVertexBuffer& StaticVertColors = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].ColorVertexBuffer;
		TArray<FVector> ProcNormalsBuffer;
		TArray<FVector2D> ProcUVsBuffer;
		TArray<FColor> ProcVertColors;
		StaticVertColors.GetVertexColors(ProcVertColors);
		TArray<FProcMeshTangent> ProcTangents;
		for(uint32 i = 0 ; i < StaticVerts.GetNumVertices() ; ++i)
		{
			ProcNormalsBuffer.Emplace(StaticVerts.VertexTangentZ(i));
			ProcUVsBuffer.Emplace(StaticVerts.GetVertexUV(i, 0));
			FVector Tangent = StaticVerts.VertexTangentY(i);
			ProcTangents.Emplace(Tangent.X, Tangent.Y, Tangent.Z);
		}

		ProcMesh = NewObject<UProceduralMeshComponent>(GetOwner());
		ProcMesh->RegisterComponent();
		//ProcMesh->AttachTo(GetRootComponent(), SocketName /* NAME_None */);

		ProcMesh->CreateMeshSection(0, ProcVertexBuffer, ProcIndicesBuffer, ProcNormalsBuffer, ProcUVsBuffer, ProcVertColors, ProcTangents, true);
		ProcMesh->SetMaterial(0, StaticMesh->GetMaterial(0));
		
		StaticMesh->DestroyComponent(); //Remove original mesh
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StaticMeshComponent not found"));
	}
}


void UMeshShatterer::ShatterToGround()
{
	UStaticMeshComponent* StaticMesh = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if(StaticMesh != nullptr)
	{
		const FRawStaticIndexBuffer& StaticIndices = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].IndexBuffer;
		const FPositionVertexBuffer& StaticVertPositions = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].PositionVertexBuffer;
		const FStaticMeshVertexBuffer& StaticVerts = StaticMesh->GetStaticMesh()->RenderData->LODResources[0].VertexBuffer;
		const FIndexArrayView& StaticIndicesBuffer = StaticIndices.GetArrayView();
		ProcMeshes.Reserve(StaticIndicesBuffer.Num() / 3);
		for(int32 i = 0 ; i < StaticIndicesBuffer.Num() ; i += 3)
		{
			TArray<FVector> ProcVertexBuffer;
			TArray<int32> ProcIndicesBuffer;
			TArray<FVector> ProcNormalsBuffer;
			TArray<FVector2D> ProcUVsBuffer;
			TArray<FColor> ProcVertColors;
			TArray<FProcMeshTangent> ProcTangents;
			
			//Get positions of the three verts
			FVector TransformedPosition = StaticMesh->GetComponentTransform().TransformPosition(StaticVertPositions.VertexPosition(StaticIndicesBuffer[i]));
			ProcVertexBuffer.Emplace(TransformedPosition);
			TransformedPosition = StaticMesh->GetComponentTransform().TransformPosition(StaticVertPositions.VertexPosition(StaticIndicesBuffer[i + 1]));
			ProcVertexBuffer.Emplace(TransformedPosition);
			TransformedPosition = StaticMesh->GetComponentTransform().TransformPosition(StaticVertPositions.VertexPosition(StaticIndicesBuffer[i + 2]));
			ProcVertexBuffer.Emplace(TransformedPosition);

			//Calculate tri area, skip small tris as optimisation
			FVector ZeroToOneSize = ProcVertexBuffer[1] - ProcVertexBuffer[0];
			FVector ZeroToTwoSize = ProcVertexBuffer[2] - ProcVertexBuffer[0];
			ZeroToOneSize.Normalize();
			ZeroToTwoSize.Normalize();
			float Dot = FVector::DotProduct(ZeroToOneSize, ZeroToTwoSize);
			float TriArea = FMath::Acos(Dot);

			if(TriArea > 0.42f)
			{
				for(int32 j = 0 ; j < 3 ; ++j)
				{
					uint32 Index = StaticIndicesBuffer[i + j];

					ProcIndicesBuffer.Emplace(j);

					ProcNormalsBuffer.Emplace(StaticVerts.VertexTangentZ(Index));
					ProcUVsBuffer.Emplace(StaticVerts.GetVertexUV(Index, 0));

					ProcVertColors.Emplace(FColor::White);

					FVector Tangent = StaticVerts.VertexTangentY(Index);
					ProcTangents.Emplace(Tangent.X, Tangent.Y, Tangent.Z);
				}
				//Create back face
				ProcIndicesBuffer.Emplace(2);
				ProcIndicesBuffer.Emplace(1);
				ProcIndicesBuffer.Emplace(0);

				//Create new component
				int32 NewIndex = ProcMeshes.Emplace();
				ProcMeshes[NewIndex] = NewObject<UProceduralMeshComponent>(GetOwner());
				//Set physics settings
				ProcMeshes[NewIndex]->bUseComplexAsSimpleCollision = false;
				ProcMeshes[NewIndex]->bGenerateOverlapEvents = false;
				ProcMeshes[NewIndex]->SetSimulatePhysics(true);

				//Create a triangular prism for collision mesh
				TArray<FVector> ProcCollisionMesh;
				FVector ZeroToOne = ProcVertexBuffer[1] - ProcVertexBuffer[0];
				FVector ZeroToTwo = ProcVertexBuffer[2] - ProcVertexBuffer[0];
				FVector SideDir = FVector::CrossProduct(ZeroToOne, ZeroToTwo);
				SideDir.Normalize();
				SideDir * 2.0f;
				for(uint8 i = 0 ; i < 3 ; ++i)
				{
					ProcCollisionMesh.Emplace(ProcVertexBuffer[i] + SideDir);
					ProcCollisionMesh.Emplace(ProcVertexBuffer[i] - SideDir);
				}
				ProcMeshes[NewIndex]->AddCollisionConvexMesh(ProcCollisionMesh);

				ProcMeshes[NewIndex]->RegisterComponent();

				ProcMeshes[NewIndex]->CreateMeshSection(0, ProcVertexBuffer, ProcIndicesBuffer, ProcNormalsBuffer, ProcUVsBuffer, ProcVertColors, ProcTangents, true);
				ProcMeshes[NewIndex]->SetMaterial(0, StaticMesh->GetMaterial(0));

				FVector ActorCenter = GetOwner()->GetTransform().GetLocation();
				ProcMeshes[NewIndex]->AddRadialForce(ActorCenter, 120.0f, 9000.0f, ERadialImpulseFalloff::RIF_Linear);
			}
		}

		StaticMesh->DestroyComponent(); //Remove original mesh
	}
}