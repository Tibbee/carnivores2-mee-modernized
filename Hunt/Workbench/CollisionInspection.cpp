#include "Hunt.h"
#include "CollisionInspection.h"
#include "TuningRuntime.h"
#include <algorithm>

namespace workbench {
namespace {
bool InMap(int x,int z) { return x>=0 && z>=0 && x<ctMapSize && z<ctMapSize; }
bool PlayerCell(int& x,int& z) {
	if(!RuntimeSession().IsLoaded() || !CollisionFinite(PlayerY) ||
		!CollisionFinite(PlayerX)||!CollisionFinite(PlayerZ)||PlayerX<0||PlayerZ<0||
		PlayerX>=ctMapSize*256.f||PlayerZ>=ctMapSize*256.f)return false;
	x=static_cast<int>(PlayerX)/256;z=static_cast<int>(PlayerZ)/256;return true;
}
// Loaded slot lifetime, not a distance window. OCount is a model-loader
// scratch count (internal .3df records) and is deliberately not consulted;
// release clears the owned model before freeing data.
bool ObjectAt(int x,int z,int object) {
	return InMap(x,z)&&object>=0&&object<254&&OMap[z][x]==object&&MObjects[object].model!=nullptr;
}
}
// A selection is an identity the operator picked, so walking away from it must
// not discard it: F8 exists precisely to walk around an object and compare the
// wire with what can actually be crossed. Only resource lifetime ends a
// selection -- a released slot, a cleared placement, a restarted hunt -- and
// every one of those reports its status instead of clearing silently.
NearbyCollisions ReadNearbyCollisions() {
	NearbyCollisions result;int x,z;if(!PlayerCell(x,z))return result;
	for(int cz=(std::max)(0,z-4);cz<=(std::min)(ctMapSize-1,z+4);++cz)
		for(int cx=(std::max)(0,x-4);cx<=(std::min)(ctMapSize-1,x+4);++cx) {
			const int object=OMap[cz][cx];if(!ObjectAt(cx,cz,object))continue;
			const float dx=cx*256.f+128-PlayerX,dz=cz*256.f+128-PlayerZ;
			result.objects[result.count++]={RuntimeSession().Generation(),cx,cz,object,
				MObjects[object].info.flags,(FMap[cz][cx]>>2)&3,std::sqrt(dx*dx+dz*dz)};
		}
	std::sort(result.objects.begin(),result.objects.begin()+result.count,[](const CollisionSelection& a,const CollisionSelection& b){
		if(a.distance!=b.distance)return a.distance<b.distance;
		return a.z==b.z ? a.x<b.x : a.z<b.z;
	});return result;
}
CollisionSnapshot ReadCollision(const CollisionSelection& selected, bool queryHeights) {
	CollisionSnapshot result;for(auto& b:result.bounds)b.a=-1;
	int px,pz;
	if(!PlayerCell(px,pz)){result.status=SelectionStatus::NoPlayerCell;return result;}
	if(selected.generation!=RuntimeSession().Generation()){
		result.status=SelectionStatus::StaleGeneration;return result;
	}
	if(!ObjectAt(selected.x,selected.z,selected.object)){
		// An out-of-range or released slot is a resource problem (short-circuit
		// keeps a default-selected -1 index out of MObjects); anything else with
		// an in-range slot is a placement the map no longer holds.
		const bool slotGone = selected.object<0||selected.object>=254||
			MObjects[selected.object].model==nullptr;
		result.status = (!InMap(selected.x,selected.z)||slotGone)
			? SelectionStatus::UnloadedSlot : SelectionStatus::NotPlaced;
		return result;
	}
	result.status=SelectionStatus::Valid;
	const auto& object=MObjects[selected.object];
	result.valid=true;result.flags=object.info.flags;result.bounded=(object.info.flags&ofBOUND)!=0;
	result.rotation=(FMap[selected.z][selected.x]>>2)&3;
	result.origin={selected.x*256.f+128,GetLandOH(selected.x,selected.z),selected.z*256.f+128};
	if(result.bounded) {
		for(int i=0;i<8;++i)if(ValidInspectionBound(object.bound[i])) {
			result.bounds[i]=object.bound[i];result.active[i]=true;
		}
	}
	if(!queryHeights)return result;
	if(result.bounded) {
		float floor,ceiling;
		result.floorKnown=PointOnBound(floor,PlayerX,PlayerZ,result.origin.x,result.origin.z,result.origin.y,result.bounds.data(),result.rotation)!=FALSE;
		result.ceilingKnown=PointUnBound(ceiling,PlayerX,PlayerZ,result.origin.x,result.origin.z,result.origin.y,result.bounds.data(),result.rotation)!=FALSE;
		if(result.floorKnown)result.floor=result.origin.y+floor;
		if(result.ceilingKnown)result.ceiling=result.origin.y+ceiling;
	}
	// Legacy query consumers have unchecked +/-4 reads; never call at map edges
	// or when a neighbor index violates the loaded roster (e.g. partial reload).
	bool safe=px>=4&&pz>=4&&px<ctMapSize-4&&pz<ctMapSize-4;
	if(safe)for(int z=pz-4;z<=pz+4;++z)for(int x=px-4;x<=px+4;++x)
		if(OMap[z][x]!=255&&!ObjectAt(x,z,OMap[z][x]))safe=false;
	if(safe) {
		result.sceneFloor=GetLandQH(PlayerX,PlayerZ);
		result.sceneCeiling=GetLandCeilH(PlayerX,PlayerZ);
		result.sceneKnown=CollisionFinite(result.sceneFloor)&&CollisionFinite(result.sceneCeiling);
	}
	return result;
}
InspectionCamera ReadInspectionCamera() {
	return {CameraX,CameraY,CameraZ,ca,sa,cb,sb,CameraW,CameraH,
		static_cast<float>(VideoCX),static_cast<float>(VideoCY),WinW,WinH};
}
}
