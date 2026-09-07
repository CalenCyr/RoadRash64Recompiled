#include "hle/rt64_rr64_translation_rejection.h"
#include <limits>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <algorithm>
#include <cstring>
using namespace RT64;
#include "fixtures/rr64_matching_workload_fixture.inc"
static void check(bool x,const char*why){if(!x){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static GameFrame frame(unsigned slot){GameFrame f;f.workloads={slot};f.perspectiveScenes.push_back(GameScene{{{slot,0,0}}});f.frameMap.workloads.resize(WORKLOAD_QUEUE_SIZE);return f;}
static uint64_t digest=1469598103934665603ull;
static void mix(uint64_t value){digest=(digest^value)*1099511628211ull;}
static void real(float value){uint32_t bits;std::memcpy(&bits,&value,4);mix(bits);}
template<class Map>static void mapping(const Map&m){
 mix(m.mapped);mix(m.prevTransformIndex);const auto&r=m.rigidBody;
 for(unsigned i=0;i<3;++i)real(r.linearVelocity[i]);real(r.angularVelocity);mix(r.transformIndex);
 mix(r.lerpTranslation);mix(r.lerpRotation);mix(r.lerpScale);mix(r.lerpSkew);mix(r.lerpPerspective);mix(r.lerpDecompose);
 for(const auto&t:r.transforms){mix(t.valid);mix(t.coordinateFlip);if(t.valid){for(unsigned i=0;i<3;++i){real(t.scale[i]);real(t.skew[i]);real(t.translation[i]);}for(unsigned i=0;i<4;++i)real(t.perspective[i]);}}
 auto identity=hlslpp::float4x4::identity();auto mid=r.lerp(0.5f,identity,identity,false);for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)real(mid[row][col]);
}
static void match(GameFrame &a,const GameFrame &b,WorkloadQueue&q){
 bool v=0,t=0,l=0;a.match(nullptr,q,b,nullptr,v,t,l);check(!v&&!t&&!l,"Unexpected uploader");
 mix(a.matched);mix(a.rr64InterpolationCompatible);mix(v);mix(t);mix(l);
 for(const auto&w:a.frameMap.workloads){mix(w.mapped);mix(w.prevWorkloadIndex);for(const auto&t:w.transforms)mapping(t);for(const auto&t:w.viewProjections)mapping(t);for(bool v:w.prevTransformsMapped)mix(v);}
}
int main(){
 _putenv_s("RR64_STABLE_PRESENTATION","1");auto q=std::make_unique<WorkloadQueue>();unsigned proven=0,deferred=0;
 for(unsigned count:{1u,7u,32u})for(float dx:{0.f,1.f,4.9f,5.f,8.f,50.f,-8.f})for(float camera:{0.f,1.f,-2.f})for(int history:{0,1,2})for(unsigned policy:{G_EX_COMPONENT_AUTO,G_EX_COMPONENT_INTERPOLATE,G_EX_COMPONENT_SKIP}){
  seed(q->workloads[0],count,8,0);seed(q->workloads[1],count,8,0);auto prev=frame(0),cur=frame(1);
  q->workloads[1].drawData.worldTransforms[0][3][0]=dx;q->workloads[1].drawData.viewTransforms[0][3][0]=camera;
  q->workloads[1].drawData.transformGroups[1].positionInterpolation=policy;
  if(history){prev.matched=true;auto&m=prev.frameMap.workloads[0];m.mapped=true;m.transforms.resize(count);m.viewProjections.resize(1);m.transforms[0].rigidBody.linearVelocity={history==1?8.f:-8.f,0,0};prev.buildTransformIdMap(q->workloads[0],q->workloads[0].transformIdMap,q->workloads[0].transformIgnoredIds);}
  auto proof=RR64TranslationRejection::reject(true,cur,prev,*q);check(!RR64TranslationRejection::reject(false,cur,prev,*q),"Ineligible path must defer");
  match(cur,prev,*q);if(proof){check(!cur.rr64InterpolationCompatible,"False positive: actual matcher accepted");++proven;}else ++deferred;
 }
 // A rejected AUTO transition records history that allows the next one.
 seed(q->workloads[0],7,8,0);seed(q->workloads[1],7,8,0);seed(q->workloads[2],7,8,0);
 auto a=frame(0),b=frame(1),c=frame(2);
 q->workloads[1].drawData.worldTransforms[0][3][0]=8;q->workloads[1].drawData.viewTransforms[0][3][0]=-2;
 q->workloads[2].drawData.worldTransforms[0][3][0]=16;q->workloads[2].drawData.viewTransforms[0][3][0]=-4;
 check(RR64TranslationRejection::reject(true,b,a,*q),"Abrupt first movement must be provably rejected");match(b,a,*q);
 check(!RR64TranslationRejection::reject(true,c,b,*q),"Recorded velocity must let steady next movement recover");match(c,b,*q);check(c.rr64InterpolationCompatible,"Actual matcher must recover");
 b.matched=false;auto d=frame(2);check(RR64TranslationRejection::reject(true,d,b,*q),"Skipping previous full match would wrongly make rejection sticky");
 // Boundaries intentionally defer: nonunique identities, AUTO objects,
 // unmatched camera policy, and any possible unchanged previous camera.
 for(unsigned mode:{0u,1u,2u,3u,4u,6u,8u,10u,11u,12u,15u}){
  seed(q->workloads[0],7,8,mode);seed(q->workloads[1],7,8,mode);auto p=frame(0),n=frame(1);
  q->workloads[1].drawData.worldTransforms[0][3][0]=8;q->workloads[1].drawData.viewTransforms[0][3][0]=-2;
  bool proof=RR64TranslationRejection::reject(true,n,p,*q);match(n,p,*q);if(proof){check(!n.rr64InterpolationCompatible,"Adversarial false positive");++proven;}else ++deferred;
 }
 for(unsigned twist=0;twist<8;++twist)for(unsigned count:{2u,7u,32u}){
  seed(q->workloads[0],count,8,11);seed(q->workloads[1],count,8,11);auto p=frame(0),n=frame(1);
  for(unsigned w=0;w<2;++w){auto&g=q->workloads[w].drawData.transformGroups[1];g.matrixId=0x52510000;g.ordering=G_EX_ORDER_LINEAR;}
  auto& cd=q->workloads[1].drawData;
  cd.worldTransforms[0][3][0]=8;cd.viewTransforms[0][3][0]=-2;
  if(twist==1)cd.worldTransforms[1][3][0]=3;
  if(twist==2)cd.posFloats[cd.worldTransformVertexIndices[1]*3]+=1;
  if(twist==3)std::swap(cd.faceIndices[8*3+1],cd.faceIndices[8*3+2]);
  if(twist==4)cd.posFloats[cd.worldTransformVertexIndices[1]*3]=std::numeric_limits<float>::quiet_NaN();
  if(twist==5)cd.transformGroups[2].matrixId=G_EX_ID_IGNORE;
  if(twist==6){cd.viewTransforms.push_back(hlslpp::float4x4::identity());cd.projTransforms.push_back(cd.projTransforms[0]);cd.viewProjTransforms.push_back(cd.viewProjTransforms[0]);cd.viewProjTransformGroups.push_back(0);auto&pair=q->workloads[1].fbPairs[0];pair.projections.push_back(pair.projections[0]);pair.projections[1].transformsIndex=1;pair.projectionCount=2;n.perspectiveScenes.push_back(GameScene{{{1,0,1}}});}
  if(twist==7){auto&pd=q->workloads[0].drawData;pd.viewTransforms.push_back(cd.viewTransforms[0]);pd.projTransforms.push_back(pd.projTransforms[0]);pd.viewProjTransforms.push_back(pd.viewProjTransforms[0]);pd.viewProjTransformGroups.push_back(0);auto&pair=q->workloads[0].fbPairs[0];pair.projections.push_back(pair.projections[0]);pair.projections[1].transformsIndex=1;pair.projectionCount=2;p.perspectiveScenes.push_back(GameScene{{{0,0,1}}});}
  bool proof=RR64TranslationRejection::reject(true,n,p,*q);match(n,p,*q);if(proof){check(!n.rr64InterpolationCompatible,"Mixed AUTO/LINEAR false positive");++proven;}else ++deferred;
 }
 for(unsigned special=0;special<4;++special){
  seed(q->workloads[0],7,8,0);seed(q->workloads[1],7,8,0);auto p=frame(0),n=frame(1);
  auto&pd=q->workloads[0].drawData;auto&cd=q->workloads[1].drawData;
  cd.worldTransforms[0][3][0]=8;cd.viewTransforms[0][3][0]=-2;
  if(special==0)for(auto&call:q->workloads[0].fbPairs[0].projections[0].gameCalls)call.callDesc.triangleCount=0;
  if(special==1)q->workloads[1].paused=true;
  if(special==2)cd.transformGroups[0].matrixId=0x77770000;
  if(special==3){cd.transformGroups[2].matrixId=cd.transformGroups[1].matrixId;pd.transformGroups[2].matrixId=pd.transformGroups[1].matrixId;}
  bool proof=RR64TranslationRejection::reject(true,n,p,*q);match(n,p,*q);if(proof){check(!n.rr64InterpolationCompatible,"Special proof false positive");++proven;}else ++deferred;
  if(special==0)check(n.rr64GeometryRejectionReasons&4,"Unpaired scene must retain membership reason, not invented translation reason");
 }
 // Full matching trusts the previous ID map whenever previous.matched is set.
 // Independently reconstructing an assumed map must not prove rejection.
 for(unsigned malformed=0;malformed<3;++malformed){
  seed(q->workloads[0],7,8,0);seed(q->workloads[1],7,8,0);auto p=frame(0),n=frame(1);p.matched=true;
  auto &cd=q->workloads[1].drawData;cd.worldTransforms[0][3][0]=8;cd.viewTransforms[0][3][0]=-2;
  auto &ids=q->workloads[0].transformIdMap;
  if(malformed==1)ids.emplace(0x52510000u,1u);
  if(malformed==2){ids.emplace(0x52510000u,0u);ids.emplace(0x52510000u,1u);}
  check(!RR64TranslationRejection::reject(true,n,p,*q),"Malformed trusted ID map must defer");match(n,p,*q);++deferred;
 }
 // A different current scene can copy its SKIP camera mapping onto an AUTO
 // view. Its world/view translations then both hold and are coherent.
 seed(q->workloads[0],1,8,0);seed(q->workloads[1],1,8,0);auto multiPrev=frame(0),multiCur=frame(1);
 for(unsigned w=0;w<2;++w){
  auto &d=q->workloads[w].drawData;auto&pair=q->workloads[w].fbPairs[0];
  TransformGroup camera;camera.matrixId=0x77770000;camera.positionInterpolation=G_EX_COMPONENT_SKIP;d.transformGroups.push_back(camera);
  d.transformGroups[1].positionInterpolation=G_EX_COMPONENT_SKIP;
  d.viewTransforms.resize(2,hlslpp::float4x4::identity());d.projTransforms.resize(2,hlslpp::float4x4::identity());d.viewProjTransforms.resize(2,hlslpp::float4x4::identity());d.viewProjTransformGroups={2,0};
  pair.projections.push_back(pair.projections[0]);pair.projections[1].transformsIndex=1;pair.projectionCount=2;
  if(w){d.worldTransforms[0][3][0]=8;d.viewTransforms[0][3][0]=-2;d.viewTransforms[1][3][0]=-2;}
 }
 multiPrev.perspectiveScenes={GameScene{{{0,0,0}}},GameScene{{{0,0,1}}}};
 multiCur.perspectiveScenes={GameScene{{{1,0,0},{1,0,1}}},GameScene{{{1,0,1}}}};
 check(!RR64TranslationRejection::reject(true,multiCur,multiPrev,*q),"Any multi-projection current scene must defer");match(multiCur,multiPrev,*q);
 std::printf("copied-camera reasons=%u world=%u view0=%u view1=%u mapped=%u/%u/%u\n",multiCur.rr64GeometryRejectionReasons,multiCur.frameMap.workloads[1].transforms[0].rigidBody.lerpTranslation,multiCur.frameMap.workloads[1].viewProjections[0].rigidBody.lerpTranslation,multiCur.frameMap.workloads[1].viewProjections[1].rigidBody.lerpTranslation,multiCur.frameMap.workloads[1].transforms[0].mapped,multiCur.frameMap.workloads[1].viewProjections[0].mapped,multiCur.frameMap.workloads[1].viewProjections[1].mapped);
 check(multiCur.frameMap.workloads[1].viewProjections[1].mapped &&
     !multiCur.frameMap.workloads[1].viewProjections[1].rigidBody.lerpTranslation,
     "Actual AUTO view must inherit the other scene's held camera mapping");++deferred;
 seed(q->workloads[0],2000,80,0);seed(q->workloads[1],2000,80,0);auto p=frame(0),n=frame(1);
 q->workloads[1].drawData.worldTransforms[0][3][0]=8;q->workloads[1].drawData.viewTransforms[0][3][0]=-2;
 std::vector<double>times;
 for(unsigned i=0;i<70;++i){auto start=std::chrono::steady_clock::now();check(RR64TranslationRejection::reject(true,n,p,*q),"Timed proof failed");if(i>=10)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
 std::sort(times.begin(),times.end());std::printf("PASS actual-R22-oracles=%u proven=%u deferred=%u history-recovery=1 proof-median-ms=%.6f digest=%llu\n",proven+deferred,proven,deferred,times[times.size()/2],(unsigned long long)digest);
 std::vector<double> matchtimes;for(unsigned i=0;i<35;++i){auto f=frame(1);auto start=std::chrono::steady_clock::now();match(f,p,*q);if(i>=5)matchtimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}std::sort(matchtimes.begin(),matchtimes.end());
 std::printf("matching-median-ms=%.6f\n",matchtimes[matchtimes.size()/2]);
}
