//
//  TerrainGenerator.h
//  UE4VoxelTerrain
//
//  Created by Admin on 19.04.2020.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

#pragma once

//#include "EngineMinimal.h"
#include <unordered_map>
#include "VoxelIndex.h"
#include "SandboxVoxeldata.h"
//#include "SandboxTerrainController.h"
//#include "TerrainZoneComponent.h"
#include "perlin.hpp"
#include <algorithm>
#include <math.h>
#include <cstring> 

class ASandboxTerrainController;
class TVoxelData;
class UTerrainZoneComponent;
struct FSandboxFoliage;

#define USBT_VGEN_GRADIENT_THRESHOLD        400
#define USBT_VD_MIN_DENSITY                 0.003    // minimal density = 1f/255
#define USBT_VGEN_GROUND_LEVEL_OFFSET       205.f


class TZoneHeightMapData {
    
private:
    
    int Size;
    float* HeightLevelArray;
    float MaxHeightLevel = -999999.f;
    float MinHeightLevel = 999999.f;
    
public:
    
    TZoneHeightMapData(int Size){
        this->Size = Size;
        HeightLevelArray = new float[Size * Size * Size];
    }
    
    ~TZoneHeightMapData(){
        delete[] HeightLevelArray;
    }

	float const* const GetHeightLevelArrayPtr() const {
		return HeightLevelArray;
	}
    
    FORCEINLINE void SetHeightLevel(const int X, const int Y, float HeightLevel){
        if(X < Size && Y < Size){
            int Index = X * Size + Y;
            HeightLevelArray[Index] = HeightLevel;
            
            if(HeightLevel > this->MaxHeightLevel){
                this->MaxHeightLevel = HeightLevel;
            }
            
            if(HeightLevel < this->MinHeightLevel){
                this->MinHeightLevel = HeightLevel;
            }
        }
    }
    
    FORCEINLINE float GetHeightLevel(const int X, const int Y) const{
        if(X < Size && Y < Size){
			int Index = X * Size + Y;
            return HeightLevelArray[Index];
        } else {
            return 0;
        }
    }
    
    FORCEINLINE float GetMaxHeightLevel() const {
        return this->MaxHeightLevel;
        
    };
    
    FORCEINLINE float GetMinHeightLevel() const {
        return this->MinHeightLevel;
        
    };
};


#define VdLinearSize 65 * 65 * 65


class TTerrainGenerator {

private:
	int VdNum;
    PerlinNoise Pn;
    ASandboxTerrainController* Controller;
    TArray<FTerrainUndergroundLayer> UndergroundLayersTmp;
	std::mutex ZoneHeightMapMutex;
    std::unordered_map<TVoxelIndex, TZoneHeightMapData*> ZoneHeightMapCollection;

	TVoxelData* VdTmp = nullptr;

	struct TTerrainGeneratorPrebuiltData {
		TVoxelIndex Index;
		FVector LocalPos;
	};

    
public:
    
    TTerrainGenerator(ASandboxTerrainController* Controller){
        this->Controller = Controller;
    }
    
	float PerlinNoise(float X, float Y, float Z) {
		return Pn.noise(X, Y, Z);
	}

	float PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) {
		return Pn.noise(Pos.X * PositionScale, Pos.Y * PositionScale, Pos.Z * PositionScale) * ValueScale;
	}

    void OnBeginPlay(){
        UndergroundLayersTmp.Empty();
        
        if(this->Controller->TerrainParameters && this->Controller->TerrainParameters->UndergroundLayers.Num() > 0){
            this->UndergroundLayersTmp = this->Controller->TerrainParameters->UndergroundLayers;
        } else {
            FTerrainUndergroundLayer DefaultLayer;
            DefaultLayer.MatId = 1;
            DefaultLayer.StartDepth = 0;
            DefaultLayer.Name = TEXT("Dirt");
            UndergroundLayersTmp.Add(DefaultLayer);
        }
        
        FTerrainUndergroundLayer LastLayer;
        LastLayer.MatId = 0;
        LastLayer.StartDepth = 9999999.f;
        LastLayer.Name = TEXT("");
        UndergroundLayersTmp.Add(LastLayer);

		this->VdTmp = this->Controller->NewVoxelData();
		this->VdNum = VdTmp->num();
    }
    
	~TTerrainGenerator() {
		if (this->VdTmp) {
			delete VdTmp;
		}
	}

private:
    
    FORCEINLINE float ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const {
		const float Z = V.Z;
		const float D = Z - GroundLevel;

		if (D > 500) {
			return 0.f;
		}

		if (D < -500) {
			return 1.f;
		}

		float DensityByGroundLevel = 1 - (1 / (1 + exp(-(Z - GroundLevel)/20)));
		return DensityByGroundLevel;
    }

    FORCEINLINE float DensityFunc(TVoxelDensityFunctionData& FunctionData){
        return Controller->GeneratorDensityFunc(FunctionData);
    }

	FORCEINLINE unsigned char MaterialFunc(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel){
		const float DeltaZ = WorldPos.Z - GroundLevel;

        //FVector test = FVector(WorldPos);
        //test.Z += 30;
        //float densityUpper = ClcDensityByGroundLevel(test, GroundLevel);

        unsigned char mat = 0;
        if (abs(DeltaZ) < 25) {
            mat = 2; // grass
        } else {
            FTerrainUndergroundLayer* Layer = GetUndergroundMaterialLayer(WorldPos.Z, GroundLevel);
            if (Layer != nullptr) {
                mat = Layer->MatId;
            }
        }

        return mat;
    }

//#include "simd.h"

    void GenerateZoneVolume(TVoxelIndex& ZoneIndex, TVoxelData& VoxelData, const TZoneHeightMapData* ZoneHeightMapData){
        //TSet<unsigned char> material_list;
		//std::set<int> MaterialSet;
        int zc = 0; 
		int fc = 0;

		// avx test
		// =================================================================================================================================================
		/*
		{
			const int size = 65 * 65 * 65;
			const int array_size = 274632;
			float* gl = new float[array_size];
			float* density = new float[array_size];


			float* z = new float[array_size];
			//-------------------------------------------------------------------------
			int i = 0;
			for (const auto& PrebuiltData : PrebuiltDataCache3D) {
				const FVector& LocalPos = PrebuiltData.LocalPos;
				const TVoxelIndex& Index = PrebuiltData.Index;
				FVector WorldPos = LocalPos + VoxelData.getOrigin();
				z[i] = WorldPos.Z;
				i++;
			}
			//-------------------------------------------------------------------------

			double Start = FPlatformTime::Seconds();

			int idx = 0;
			for (auto i = 0; i < 65; i++) {
				memcpy(&gl[i], ZoneHeightMapData->GetHeightLevelArrayPtr(), 65*65);
				i += 65 * 65;
			}



			float GroundLevel = 0;
			//const __m256 gl = _mm256_set1_ps(GroundLevel);
			const __m256 c20 = _mm256_set1_ps(20);
			const __m256 cn1 = _mm256_set1_ps(-1);
			const __m256 c1 = _mm256_set1_ps(1);

			for (auto i = 0; i < array_size; i += 8) {
				uSIMD u;
				memcpy(&u.a[0], &z[i], sizeof(float) * 8);

				uSIMD gl_;
				memcpy(&gl_.a[0], &gl[i], sizeof(float) * 8);

				uSIMD r;
				__m256 a = _mm256_sub_ps(u.m, gl_.m);
				a = _mm256_div_ps(a, c20);
				a = _mm256_mul_ps(a, cn1);
				__m256 b = faster_more_accurate_exp_avx2(a);
				b = _mm256_add_ps(b, c1);
				__m256 c = _mm256_div_ps(c1, b);
				r.m = _mm256_sub_ps(c1, c);

				memcpy(&density[i], &r.a, sizeof(float) * 8);
			}



			double End = FPlatformTime::Seconds();
			double Time = (End - Start) * 1000;
			UE_LOG(LogTemp, Warning, TEXT("TEST ----> %f ms --  %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
		}
		*/
		// =================================================================================================================================================


		double Start = FPlatformTime::Seconds();

		//int i = 0;
		//unsigned char M = 0;
		bool bContainsMoreOneMaterial = false;
		unsigned char BaseMaterialId = 0;

		for (int X = 0; X < VdTmp->num(); X++) {
			for (int Y = 0; Y < VdTmp->num(); Y++) {
				for (int Z = 0; Z < VdTmp->num(); Z++) {
					const FVector& LocalPos = VdTmp->voxelIndexToVector(X, Y, Z);
					const TVoxelIndex& Index = TVoxelIndex(X, Y, Z);

					FVector WorldPos = LocalPos + VoxelData.getOrigin();
					float GroundLevel = ZoneHeightMapData->GetHeightLevel(Index.X, Index.Y);

					TVoxelDensityFunctionData FunctionData;
					FunctionData.Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
					//FunctionData.Density = density[i];
					FunctionData.ZoneIndex = ZoneIndex;
					FunctionData.WorldPos = WorldPos;
					FunctionData.LocalPos = LocalPos;

					float Density = DensityFunc(FunctionData);
					unsigned char MaterialId = MaterialFunc(LocalPos, WorldPos, GroundLevel);

					VoxelData.setDensity(Index.X, Index.Y, Index.Z, Density);
					VoxelData.setMaterial(Index.X, Index.Y, Index.Z, MaterialId);

					VoxelData.performSubstanceCacheLOD(Index.X, Index.Y, Index.Z);

					if (Density == 0) {
						zc++;
					}

					if (Density == 1) {
						fc++;
					}

					if (!BaseMaterialId) {
						BaseMaterialId = MaterialId;
					} else {
						if (BaseMaterialId != MaterialId) {
							bContainsMoreOneMaterial = true;
						}
					}

					BaseMaterialId = MaterialId;
				}
			}
		}

		double End = FPlatformTime::Seconds();
		double Time = (End - Start) * 1000;
		UE_LOG(LogTemp, Warning, TEXT("GenerateZoneVolume 1 ----> %f ms --  %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);


        int s = VoxelData.num() * VoxelData.num() * VoxelData.num();

        if (zc == s) {
            VoxelData.deinitializeDensity(TVoxelDataFillState::ZERO);
        }

        if (fc == s) {
            VoxelData.deinitializeDensity(TVoxelDataFillState::FULL);
        }

		if (!bContainsMoreOneMaterial) {
			VoxelData.deinitializeMaterial(BaseMaterialId);
		}

    }

    FTerrainUndergroundLayer* GetUndergroundMaterialLayer(float Z, float RealGroundLevel){
        for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
            FTerrainUndergroundLayer& Layer = UndergroundLayersTmp[Idx];
            if (Z <= RealGroundLevel - Layer.StartDepth && Z > RealGroundLevel - UndergroundLayersTmp[Idx + 1].StartDepth) {
                return &Layer;
            }
        }
        return nullptr;
    }

    int GetAllUndergroundMaterialLayers(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList){
        static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
        float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize;
        float ZoneLow = ZoneOrigin.Z - ZoneHalfSize;
        float TerrainHigh = ZoneHeightMapData->GetMaxHeightLevel();
        float TerrainLow = ZoneHeightMapData->GetMinHeightLevel();

        int Cnt = 0;
        for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
            FTerrainUndergroundLayer& Layer1 = UndergroundLayersTmp[Idx];
            FTerrainUndergroundLayer& Layer2 = UndergroundLayersTmp[Idx + 1];

            float LayerHigh = TerrainHigh - Layer1.StartDepth;
            float LayerLow = TerrainLow - Layer2.StartDepth;

            if (std::max(ZoneLow, LayerLow) < std::min(ZoneHigh, LayerHigh)) {
                Cnt++;
                if (LayerList) {
                    LayerList->Add(Layer1);
                }
            }
        }

        return Cnt;
    }

    FORCEINLINE bool IsZoneOnGroundLevel(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin){
        static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
        float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize + 500;
        float ZoneLow = ZoneOrigin.Z - ZoneHalfSize - 10;
        float TerrainHigh = ZoneHeightMapData->GetMaxHeightLevel();
        float TerrainLow = ZoneHeightMapData->GetMinHeightLevel();
        return std::max(ZoneLow, TerrainLow) <= std::min(ZoneHigh, TerrainHigh);
    }

    bool IsZoneOverGroundLevel(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin){
        static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
        return ZoneHeightMapData->GetMaxHeightLevel() < ZoneOrigin.Z - ZoneHalfSize;
    }
    
    
	TZoneHeightMapData* GetZoneHeightMap(int X, int Y) {
		const std::lock_guard<std::mutex> lock(ZoneHeightMapMutex);

		TVoxelIndex Index(X, Y, 0);
		TZoneHeightMapData* ZoneHeightMapData = nullptr;

		if (ZoneHeightMapCollection.find(Index) == ZoneHeightMapCollection.end()) {
			ZoneHeightMapData = new TZoneHeightMapData(VdNum);
			ZoneHeightMapCollection.insert({ Index, ZoneHeightMapData });


			double Start = FPlatformTime::Seconds();

			for (int X = 0; X < VdTmp->num(); X++) {
				for (int Y = 0; Y < VdTmp->num(); Y++) {
					const FVector& LocalPos = VdTmp->voxelIndexToVector(X, Y, 0);
					FVector WorldPos = LocalPos + Controller->GetZonePos(Index);
					float GroundLevel = GroundLevelFunc(Index, WorldPos);
					ZoneHeightMapData->SetHeightLevel(X, Y, GroundLevel);
				}
			}

			double End = FPlatformTime::Seconds();
			double Time = (End - Start) * 1000;
			UE_LOG(LogTemp, Warning, TEXT("generate height map  ----> %f ms --  %d %d"), Time, X, Y);

		} else {
			ZoneHeightMapData = ZoneHeightMapCollection[Index];
		}

		return ZoneHeightMapData;
	};


public:

	std::list<TVoxelIndex> GetLandscapeZones(int X, int Y) {
		std::list<TVoxelIndex> Res;
		TZoneHeightMapData* ZoneHeightMapData = GetZoneHeightMap(X, Y);

		float Max = ZoneHeightMapData->GetMaxHeightLevel();
		float Min = ZoneHeightMapData->GetMinHeightLevel();

		FVector ZonePos = Controller->GetZonePos(TVoxelIndex(X, Y, 0));
		FVector ZonePos1 = ZonePos;
		FVector ZonePos2 = ZonePos;

		ZonePos1.Z = Max;
		ZonePos2.Z = Min;

		TVoxelIndex Index1 = Controller->GetZoneIndex(ZonePos1);
		TVoxelIndex Index2 = Controller->GetZoneIndex(ZonePos2);

		int Z = Index1.Z;
		do { 
			TVoxelIndex Index(X, Y, Z);
			Res.push_back(Index);
			Z--;
		} while (Z >= Index2.Z);

		//UE_LOG(LogTemp, Log, TEXT("Index1 %d %d %d - Index2 %d %d %d"), Index1.X, Index1.Y, Index1.Z, Index2.X, Index2.Y, Index2.Z);
		return Res;
	}

   
    void GenerateVoxelTerrain(TVoxelData& VoxelData){
        double start = FPlatformTime::Seconds();

        TVoxelIndex ZoneIndex = Controller->GetZoneIndex(VoxelData.getOrigin());
        FVector ZoneIndexTmp(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

        // get heightmap data
        TVoxelIndex Index2((int)ZoneIndexTmp.X, (int)ZoneIndexTmp.Y, 0);
		TZoneHeightMapData* ZoneHeightMapData = GetZoneHeightMap(ZoneIndexTmp.X, ZoneIndexTmp.Y);

        static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
        const FVector Origin = VoxelData.getOrigin();
        
        bool bForcePerformZone = Controller->GeneratorForcePerformZone(ZoneIndex);
        if(bForcePerformZone){
            GenerateZoneVolume(ZoneIndex, VoxelData, ZoneHeightMapData);
        } else {
            if (!IsZoneOverGroundLevel(ZoneHeightMapData, Origin)) {
                TArray<FTerrainUndergroundLayer> LayerList;
                int LayersCount = GetAllUndergroundMaterialLayers(ZoneHeightMapData, Origin, &LayerList);
                bool bIsZoneOnGround = IsZoneOnGroundLevel(ZoneHeightMapData, Origin);
                if (LayersCount == 1 && !bIsZoneOnGround) {
                    // only one material
                    VoxelData.deinitializeDensity(TVoxelDataFillState::FULL);
                    VoxelData.deinitializeMaterial(LayerList[0].MatId);
                } else {
                    GenerateZoneVolume(ZoneIndex, VoxelData, ZoneHeightMapData);
                }
            } else {
                // air only
                VoxelData.deinitializeDensity(TVoxelDataFillState::ZERO);
                VoxelData.deinitializeMaterial(0);
            }
        }
            
        VoxelData.setCacheToValid();

        double end = FPlatformTime::Seconds();
        double time = (end - start) * 1000;
        //UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::generateTerrain ----> %f %f %f --> %f ms"), VoxelData.getOrigin().X, VoxelData.getOrigin().Y, VoxelData.getOrigin().Z, time);
    }

    float GroundLevelFuncLocal(const FVector& V){
        //float scale1 = 0.0035f; // small
        float scale1 = 0.001f; // small
        float scale2 = 0.0004f; // medium
        float scale3 = 0.00009f; // big

        float noise_small = Pn.noise(V.X * scale1, V.Y * scale1, 0) * 0.5f;
        float noise_medium = Pn.noise(V.X * scale2, V.Y * scale2, 0) * 5;
        float noise_big = Pn.noise(V.X * scale3, V.Y * scale3, 0) * 10;
        
        //float r = std::sqrt(V.X * V.X + V.Y * V.Y);
		//const float MaxR = 5000;
        //float t = 1 - exp(-pow(r, 2) / ( MaxR * 100));
        
        float gl = noise_small + noise_medium + noise_big;
        return (gl * 100) + USBT_VGEN_GROUND_LEVEL_OFFSET;
    }

	float GroundLevelFunc(const TVoxelIndex& Index, const FVector& V) {
		float GroundLevel = GroundLevelFuncLocal(V);

		if (Controller->IsOverrideGroundLevel(Index)) {
			return Controller->GeneratorGroundLevelFunc(Index, V, GroundLevel);
		}

		return GroundLevel;
	}


	int32 ZoneHash(const FVector& ZonePos) {
		int32 Hash = 7;
		Hash = Hash * 31 + (int32)ZonePos.X;
		Hash = Hash * 31 + (int32)ZonePos.Y;
		Hash = Hash * 31 + (int32)ZonePos.Z;

		return Hash;
	}

    void GenerateNewFoliageLandscape(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap){
		if (Controller->FoliageMap.Num() == 0) {
			return;
		}

		FVector ZonePos = Controller->GetZonePos(Index);

		float GroundLevel = GroundLevelFunc(Index, ZonePos); // TODO fix with zone on ground
		if (GroundLevel > ZonePos.Z + 500) {
			return;
		}

        int32 Hash = ZoneHash(ZonePos);
        FRandomStream rnd = FRandomStream();
        rnd.Initialize(Hash);
        rnd.Reset();

        static const float s = USBT_ZONE_SIZE / 2;
        static const float step = 25.f;

        for (auto x = -s; x <= s; x += step) {
            for (auto y = -s; y <= s; y += step) {

                FVector v(ZonePos);
                v += FVector(x, y, 0);

                for (auto& Elem : Controller->FoliageMap) {
                    FSandboxFoliage FoliageType = Elem.Value;

					if (FoliageType.Type == ESandboxFoliageType::Cave || FoliageType.Type == ESandboxFoliageType::Custom) {
						continue;
					}

                    int32 FoliageTypeId = Elem.Key;

                    if ((int)x % (int)FoliageType.SpawnStep == 0 && (int)y % (int)FoliageType.SpawnStep == 0) {
                        float Chance = rnd.FRandRange(0.f, 1.f);
						FSandboxFoliage FoliageType2 = Controller->GeneratorFoliageOverride(FoliageTypeId, FoliageType, Index, v);
						float Probability = FoliageType2.Probability;

                        if (Chance <= Probability) {
                            float r = std::sqrt(v.X * v.X + v.Y * v.Y);
                            SpawnFoliage(FoliageTypeId, FoliageType2, v, rnd, Index, ZoneInstanceMeshMap);
                        }
                    }
                }
            }
        }
    }

    void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap){
        FVector v = Origin;
        
        if (FoliageType.OffsetRange > 0) {
            float ox = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) ox = -ox; v.X += ox;
            float oy = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) oy = -oy; v.Y += oy;
        }

        bool bSpawnAccurate = false;
        bool bSpawn = false;
        FVector Location(0);
        
        if(bSpawnAccurate){
            const FVector start_trace(v.X, v.Y, v.Z + USBT_ZONE_SIZE / 2);
            const FVector end_trace(v.X, v.Y, v.Z - USBT_ZONE_SIZE / 2);
            FHitResult hit(ForceInit);
            Controller->GetWorld()->LineTraceSingleByChannel(hit, start_trace, end_trace, ECC_Visibility);
            
            bSpawn = hit.bBlockingHit && Cast<UVoxelMeshComponent>(hit.Component.Get()); //Cast<ASandboxTerrainController>(hit.Actor.Get())
            if (bSpawn) {
                Location = hit.ImpactPoint;
            }
        } else {
            bSpawn = true;
            float GroundLevel = GroundLevelFunc(Index, FVector(v.X, v.Y, 0)) - 5.5;
            Location = FVector(v.X, v.Y, GroundLevel);
        }
        
        if(bSpawn) {
            float Angle = rnd.FRandRange(0.f, 360.f);
            float ScaleZ = rnd.FRandRange(FoliageType.ScaleMinZ, FoliageType.ScaleMaxZ);
            FVector Scale = FVector(1, 1, ScaleZ);
            if(Controller->OnCheckFoliageSpawn(Index, Location, Scale)){
                FTransform Transform(FRotator(0, Angle, 0), Location, Scale);
                FTerrainInstancedMeshType MeshType;
                MeshType.MeshTypeId = FoliageTypeId;
                MeshType.Mesh = FoliageType.Mesh;
                MeshType.StartCullDistance = FoliageType.StartCullDistance;
                MeshType.EndCullDistance = FoliageType.EndCullDistance;

				auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(FoliageTypeId);
				InstanceMeshContainer.MeshType = MeshType;
				InstanceMeshContainer.TransformArray.Add(Transform);
                //Zone->SpawnInstancedMesh(MeshType, Transform);
            }
        }
    }

	void GenerateNewFoliageCustom(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
		if (Controller->FoliageMap.Num() == 0) {
			return;
		}

		if (!Controller->GeneratorUseCustomFoliage(Index)) {
			return;
		}

		FVector ZonePos = Controller->GetZonePos(Index);
		int32 Hash = ZoneHash(ZonePos);
		FRandomStream rnd = FRandomStream();
		rnd.Initialize(Hash);
		rnd.Reset();


		for (auto& Elem : Controller->FoliageMap) {
			FSandboxFoliage FoliageType = Elem.Value;

			if (FoliageType.Type != ESandboxFoliageType::Custom) {
				continue;
			}

			Vd->forEachCacheItem(0, [&](const TSubstanceCacheItem& itm) {
				uint32 x; 
				uint32 y; 
				uint32 z;

				Vd->clcVoxelIndex(itm.index, x, y, z);
				FVector WorldPos = Vd->voxelIndexToVector(x, y, z) + Vd->getOrigin();
				int32 FoliageTypeId = Elem.Key;

				FTransform Transform;
				bool bSpawn = Controller->GeneratorSpawnCustomFoliage(Index, WorldPos, FoliageTypeId, FoliageType, rnd, Transform);
				if (bSpawn) {
					FTerrainInstancedMeshType MeshType;
					MeshType.MeshTypeId = FoliageTypeId;
					MeshType.Mesh = FoliageType.Mesh;
					MeshType.StartCullDistance = FoliageType.StartCullDistance;
					MeshType.EndCullDistance = FoliageType.EndCullDistance;

					auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(FoliageTypeId);
					InstanceMeshContainer.MeshType = MeshType;
					InstanceMeshContainer.TransformArray.Add(Transform);
				}
			});
		}
	}

    void Clean(){
		const std::lock_guard<std::mutex> lock(ZoneHeightMapMutex);
		for (std::unordered_map<TVoxelIndex, TZoneHeightMapData*>::iterator It = ZoneHeightMapCollection.begin(); It != ZoneHeightMapCollection.end(); ++It) {
			delete It->second;
		}

		ZoneHeightMapCollection.clear();
    }

	void Clean(TVoxelIndex& Index) {
		/*
		const std::lock_guard<std::mutex> lock(ZoneHeightMapMutex);

		if (ZoneHeightMapCollection.find(Index) != ZoneHeightMapCollection.end()) {
			TZoneHeightMapData* HeightMapData = ZoneHeightMapCollection[Index];
			delete HeightMapData;
			ZoneHeightMapCollection.erase(Index);
			//UE_LOG(LogTemp, Warning, TEXT("Clean ZoneHeightMap ----> %d %d %d "), Index.X, Index.Y, Index.Z);
		} 
		*/
	}
        
};

