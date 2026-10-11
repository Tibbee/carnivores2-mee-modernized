#include <gtest/gtest.h>
#include "Hunt.h"
#include "Workbench/CollisionInspection.h"
#include "Workbench/TuningRuntime.h"
#include "workbench_audio_seams.h"
#include <cstring>
#include <limits>

namespace {
TBound Bound() { TBound b{};b.a=172;b.b=122;b.cx=31;b.cy=-20;b.cosph=.8f;b.sinph=.6f;
	b.y1=20;b.y2=100;b.gx=.04f;b.gz=-.3f;b.bottomGx=.25f;b.bottomGz=-.2f;return b; }
class WorkbenchCollision : public testing::Test {
protected:
	TModel loadedModel{}, bridgeModel{}; // Borrowed by slots; never heap-freed by the fixture.
	void SetUp() override {
		workbench_test::ResetAudio();workbench::ResetRuntimeForHunt();workbench::CaptureRuntimeBaseline();
		OCount=1;MObjects[0].model.reset(&loadedModel);std::memset(OMap,255,sizeof(OMap));std::memset(FMap,0,sizeof(FMap));std::memset(HMap,0,sizeof(HMap));std::memset(HMapO,0,sizeof(HMapO));
		PlayerX=PlayerZ=10*256+128;PlayerY=100;
		MObjects[0].info.flags=ofBOUND;for(auto& b:MObjects[0].bound)b.a=-1;
		OMap[10][10]=0;
	}
	void TearDown() override {
		MObjects[0].model.release();MObjects[77].model.release();
		OCount=0;std::memset(OMap,255,sizeof(OMap));
	}
};
}
TEST(WorkbenchCollisionGeometry, CornersMatchAllMapRotationsAndIndependentPlanes) {
	const auto b=Bound();const Vector3d origin{500,73,700};
	for(int rotation=0;rotation<4;++rotation) {
		const auto corners=workbench::InspectionCorners(b,origin,rotation);
		for(int i=0;i<4;++i) {
			float u,v;c2bound::LocalOf(corners[i].x,corners[i].z,origin.x,origin.z,b.cx,b.cy,b.cosph,b.sinph,rotation,u,v);
			EXPECT_NEAR(std::fabs(u),172,1e-3);EXPECT_NEAR(std::fabs(v),122,1e-3);
			EXPECT_NEAR(corners[i].y,73+b.y1+b.bottomGx*u+b.bottomGz*v,1e-3);
			EXPECT_NEAR(corners[i+4].y,73+b.y2+b.gx*u+b.gz*v,1e-3);
		}
	}
}
TEST(WorkbenchCollisionGeometry, EmptyRemaindersAndNonFiniteSlotsAreNeverConsumed) {
	auto b=Bound();b.a=-1;b.b=std::numeric_limits<float>::quiet_NaN();EXPECT_FALSE(workbench::ValidInspectionBound(b));
	b=Bound();b.gx=std::numeric_limits<float>::infinity();EXPECT_FALSE(workbench::ValidInspectionBound(b));
	b=Bound();b.cosph=2;EXPECT_FALSE(workbench::ValidInspectionBound(b));
	EXPECT_TRUE(workbench::ValidInspectionBound(Bound()));
}
TEST(WorkbenchCollisionGeometry, ProjectionMatchesLegacyCameraAndClipsNearAndViewport) {
	workbench::InspectionCamera camera;camera.cameraW=300;camera.cameraH=250;camera.centerX=320;camera.centerY=240;camera.width=640;camera.height=480;
	workbench::InspectionLine line;
	ASSERT_TRUE(workbench::ProjectInspectionLine({-10,20,-100},{10,20,-100},camera,line));
	EXPECT_FLOAT_EQ(line.x1,290);EXPECT_FLOAT_EQ(line.x2,350);EXPECT_FLOAT_EQ(line.y1,190);
	EXPECT_FALSE(workbench::ProjectInspectionLine({0,0,100},{2,0,90},camera,line));
	ASSERT_TRUE(workbench::ProjectInspectionLine({-10,0,10},{20,0,-100},camera,line));
	EXPECT_GE(line.x1,0);EXPECT_LE(line.x2,640);
	ASSERT_TRUE(workbench::ProjectInspectionLine({-10000,0,-100},{10000,0,-100},camera,line));
	EXPECT_NEAR(line.x1,0,1e-5);EXPECT_NEAR(line.x2,640,1e-5);
	EXPECT_FALSE(workbench::ProjectInspectionLine({10000,0,-100},{12000,0,-100},camera,line));
	camera.cameraW=std::numeric_limits<float>::quiet_NaN();EXPECT_FALSE(workbench::ProjectInspectionLine({0,0,-100},{10,0,-100},camera,line));
}
TEST_F(WorkbenchCollision, LoadedPaddingAgreesWithProductionFitWithoutDoublePadding) {
	std::array<TPoint3d,8> vertices{};int n=0;
	for(float y:{0.f,100.f})for(float z:{-50.f,50.f})for(float x:{-100.f,100.f})vertices[n++]={x,y,z,0,0};
	TModel model{};model.VCount=8;model.gVertex.reset(vertices.data());
	struct ReleaseBorrow {TModel& model;~ReleaseBorrow(){model.gVertex.release();}} release{model};
	OptBoundBoxFit=OptBoundBoxSlope=0;CalcBoundBox(&model,MObjects[0].bound);
	const auto snapshot=workbench::ReadCollision(workbench::ReadNearbyCollisions().objects[0]);
	ASSERT_TRUE(snapshot.active[0]);EXPECT_FLOAT_EQ(snapshot.bounds[0].a,172.5f);EXPECT_FLOAT_EQ(snapshot.bounds[0].b,122.5f);
	const auto corners=workbench::InspectionCorners(snapshot.bounds[0],snapshot.origin,0);
	EXPECT_FLOAT_EQ(corners[1].x-corners[0].x,345); // Inclusive legacy extent, no second +72.
}
TEST_F(WorkbenchCollision, AreaObjectIDDoesNotDependOnLastModelInternalObjectCount) {
	MObjects[77].model.reset(&bridgeModel);MObjects[77].info.flags=ofBOUND;
	for(auto& b:MObjects[77].bound)b.a=-1;MObjects[77].bound[0]=Bound();
	OMap[10][10]=77;
	for(int rotation=0;rotation<4;++rotation)for(int scratchCount:{0,1,8,1024}) {
		OCount=scratchCount;FMap[10][10]=static_cast<BYTE>(rotation<<2);
		const auto nearby=workbench::ReadNearbyCollisions();ASSERT_EQ(nearby.count,1u);
		EXPECT_EQ(nearby.objects[0].object,77);const auto snapshot=workbench::ReadCollision(nearby.objects[0]);
		ASSERT_TRUE(snapshot.valid);EXPECT_TRUE(snapshot.active[0]);EXPECT_EQ(snapshot.rotation,rotation);
	}
	const auto selected=workbench::ReadNearbyCollisions().objects[0];MObjects[77].model.release();
	EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);EXPECT_FALSE(workbench::ReadCollision(selected).valid);
}
TEST_F(WorkbenchCollision, SentinelCellsAndUnloadedSlotsNeverBecomePlacements) {
	OMap[10][10]=254;EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);
	OMap[10][10]=255;EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);
	OMap[10][10]=77;EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);
	OMap[10][10]=0;MObjects[0].model.release();EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);
}
TEST_F(WorkbenchCollision, NeighborhoodIsBoundedClampedAndIgnoresInvalidIndices) {
	std::memset(OMap,0,sizeof(OMap));auto nearby=workbench::ReadNearbyCollisions();EXPECT_EQ(nearby.count,81u);
	PlayerX=PlayerZ=128;nearby=workbench::ReadNearbyCollisions();EXPECT_EQ(nearby.count,25u);
	OMap[0][0]=200;EXPECT_EQ(workbench::ReadNearbyCollisions().count,24u);
	PlayerX=-1;EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);
	PlayerX=std::numeric_limits<float>::infinity();EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u);
}
TEST_F(WorkbenchCollision, SnapshotsCopyLiveBoundsAndRejectStaleCellGenerationAndReleasedSlots) {
	MObjects[0].bound[0]=Bound();const auto selected=workbench::ReadNearbyCollisions().objects[0];
	const auto first=workbench::ReadCollision(selected);ASSERT_TRUE(first.valid);ASSERT_TRUE(first.active[0]);
	MObjects[0].bound[0].a=185;EXPECT_FLOAT_EQ(first.bounds[0].a,172);EXPECT_FLOAT_EQ(workbench::ReadCollision(selected).bounds[0].a,185);
	OMap[10][10]=255;
	auto unplaced=workbench::ReadCollision(selected);
	EXPECT_FALSE(unplaced.valid);
	EXPECT_EQ(unplaced.status,workbench::SelectionStatus::NotPlaced);
	OMap[10][10]=0;
	workbench::ResetRuntimeForHunt();workbench::CaptureRuntimeBaseline();EXPECT_FALSE(workbench::ReadCollision(selected).valid);
	EXPECT_EQ(workbench::ReadCollision(selected).status,workbench::SelectionStatus::StaleGeneration);
}
// The owner walks around a selected object in F8 to compare the wire with the
// terrain, so leaving the bounded 9 x 9 scan neighborhood must not end it.
TEST_F(WorkbenchCollision, SelectionSurvivesLeavingTheNineByNineNeighborhood) {
	MObjects[0].bound[0]=Bound();const auto selected=workbench::ReadNearbyCollisions().objects[0];
	const auto atHand=workbench::ReadCollision(selected);ASSERT_TRUE(atHand.valid);ASSERT_TRUE(atHand.active[0]);
	PlayerX=PlayerZ=600*256+128; // Far outside the 9 x 9 scan, well inside the map.
	const auto walkedAway=workbench::ReadCollision(selected);
	EXPECT_TRUE(walkedAway.valid);EXPECT_EQ(walkedAway.status,workbench::SelectionStatus::Valid);
	EXPECT_FLOAT_EQ(walkedAway.bounds[0].a,172);EXPECT_EQ(walkedAway.rotation,selected.rotation);
	EXPECT_NEAR(walkedAway.origin.x,selected.x*256.f+128,1e-3);
	EXPECT_EQ(workbench::ReadNearbyCollisions().count,0u); // The list shrinks; the wire does not vanish.
	PlayerX=PlayerZ=10*256+128;
	EXPECT_TRUE(workbench::ReadCollision(selected).valid); // Returning still shows it.
}
TEST_F(WorkbenchCollision, InvalidSelectionsReportWhyInsteadOfFailingSilently) {
	MObjects[0].bound[0]=Bound();const auto selected=workbench::ReadNearbyCollisions().objects[0];
	EXPECT_EQ(workbench::ReadCollision(selected).status,workbench::SelectionStatus::Valid);
	// A cleared selection is an unloaded slot, never an out-of-range MObjects read.
	workbench::CollisionSelection unset{};unset.generation=workbench::RuntimeSession().Generation();
	EXPECT_EQ(workbench::ReadCollision(unset).status,workbench::SelectionStatus::UnloadedSlot);
	auto wrongCell=selected;wrongCell.x=11;wrongCell.z=11; // OMap[11][11] is the 255 sentinel.
	EXPECT_EQ(workbench::ReadCollision(wrongCell).status,workbench::SelectionStatus::NotPlaced);
	auto outside=selected;outside.x=ctMapSize+4;
	EXPECT_EQ(workbench::ReadCollision(outside).status,workbench::SelectionStatus::UnloadedSlot);
	MObjects[0].model.release();
	EXPECT_EQ(workbench::ReadCollision(selected).status,workbench::SelectionStatus::UnloadedSlot);
	EXPECT_FALSE(workbench::ReadCollision(selected).valid);
}
TEST_F(WorkbenchCollision, ProductionHeightsRemainSeparateFromSelectedContributionsAndRespectMargins) {
	TBound b{};b.a=b.b=300;b.cosph=1;b.y1=-30;b.y2=50;MObjects[0].bound[0]=b;
	b.y1=400;b.y2=500;MObjects[0].bound[1]=b;HMapO[10][10]=2;
	auto selected=workbench::ReadNearbyCollisions().objects[0];FMap[10][10]=3<<2;
	auto snapshot=workbench::ReadCollision(selected);ASSERT_TRUE(snapshot.sceneKnown);ASSERT_TRUE(snapshot.floorKnown);ASSERT_TRUE(snapshot.ceilingKnown);
	EXPECT_EQ(snapshot.rotation,3);EXPECT_FLOAT_EQ(snapshot.origin.y,GetLandOH(10,10));
	EXPECT_FLOAT_EQ(snapshot.floor,50+GetLandOH(10,10));EXPECT_FLOAT_EQ(snapshot.ceiling,400+GetLandOH(10,10));
	EXPECT_FLOAT_EQ(snapshot.sceneFloor,GetLandQH(PlayerX,PlayerZ));EXPECT_FLOAT_EQ(snapshot.sceneCeiling,GetLandCeilH(PlayerX,PlayerZ));
	EXPECT_FALSE(workbench::ReadCollision(selected,false).sceneKnown);
	PlayerX=PlayerZ=128;OMap[0][0]=0;selected=workbench::ReadNearbyCollisions().objects[0];
	EXPECT_TRUE(workbench::ReadCollision(selected).valid);EXPECT_FALSE(workbench::ReadCollision(selected).sceneKnown);
}
TEST_F(WorkbenchCollision, LegacyPoliciesDoNotInventBoxesAndReloadIndicesDisableQueries) {
	MObjects[0].bound[0]=Bound();MObjects[0].info.flags=ofCIRCLE;
	auto selected=workbench::ReadNearbyCollisions().objects[0];auto snapshot=workbench::ReadCollision(selected);
	ASSERT_TRUE(snapshot.valid);EXPECT_FALSE(snapshot.bounded);EXPECT_FALSE(snapshot.active[0]);
	OMap[11][11]=200;EXPECT_FALSE(workbench::ReadCollision(selected).sceneKnown);
	MObjects[0].model.release();EXPECT_FALSE(workbench::ReadCollision(selected).valid);
}
