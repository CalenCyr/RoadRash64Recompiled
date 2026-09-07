// Necessary rejection proof. This never authorizes skipping
// matching history; it only establishes that geometry certification cannot
// make the current frame eligible for interpolation.
#pragma once
#include "rt64_workload_queue.h"
#include <unordered_map>
#include <cmath>
namespace RT64::RR64TranslationRejection {
struct Unique { uint32_t index=0;unsigned count=0; };
inline bool translationDiffers(const hlslpp::float4x4 &a,const hlslpp::float4x4 &b) {
    bool changed=false;
    for(unsigned i=0;i<3;++i){float x=a[3][i],y=b[3][i];if(!std::isfinite(x)||!std::isfinite(y))return false;changed|=x!=y;}
    return changed;
}
inline const Projection *projection(const WorkloadQueue &q,uint32_t w,const GameIndices::Projection &p) {
    if(w>=q.workloads.size()||p.workloadIndex!=w)return nullptr;
    const auto &load=q.workloads[w];
    if(p.fbPairIndex>=load.fbPairCount||p.fbPairIndex>=load.fbPairs.size())return nullptr;
    const auto &pair=load.fbPairs[p.fbPairIndex];
    if(p.projectionIndex>=pair.projectionCount||p.projectionIndex>=pair.projections.size())return nullptr;
    const auto &proj=pair.projections[p.projectionIndex];
    return proj.type==Projection::Type::Perspective?&proj:nullptr;
}
inline bool explicitIds(const DrawData &d,std::unordered_map<uint32_t,Unique> &ids) {
    if(d.worldTransformGroups.size()!=d.worldTransforms.size())return false;
    for(uint32_t i=0;i<d.worldTransformGroups.size();++i){
        if(d.worldTransformGroups[i]>=d.transformGroups.size())return false;
        const auto &g=d.transformGroups[d.worldTransformGroups[i]];
        if(g.matrixId!=G_EX_ID_AUTO&&g.matrixId!=G_EX_ID_IGNORE&&g.ordering==G_EX_ORDER_LINEAR){
            auto &v=ids[g.matrixId];v.index=i;++v.count;
        }
    }
    return true;
}
inline bool reject(bool eligible,const GameFrame &cur,const GameFrame &prev,const WorkloadQueue &q) {
    if(!eligible||cur.workloads.size()!=1||prev.workloads.size()!=1||cur.perspectiveScenes.empty()||prev.perspectiveScenes.empty())return false;
    // A multi-projection scene can copy an explicit camera's held mapping
    // onto an AUTO view used by another scene. Do not assume AUTO implies
    // INTERPOLATE unless every current scene has its own one-projection path.
    for(const auto &scene:cur.perspectiveScenes)if(scene.projections.size()!=1)return false;
    auto cw=cur.workloads[0],pw=prev.workloads[0];if(cw>=q.workloads.size()||pw>=q.workloads.size())return false;
    const auto &cd=q.workloads[cw].drawData,&pd=q.workloads[pw].drawData;
    std::unordered_map<uint32_t,Unique> ci,pi;
    if(!explicitIds(cd,ci)||!explicitIds(pd,pi))return false;
    const GameFrameMap::WorkloadMap *history=nullptr;
    if(prev.matched){
        if(pw>=prev.frameMap.workloads.size())return false;
        if(prev.frameMap.workloads[pw].mapped)history=&prev.frameMap.workloads[pw];
    }
    std::vector<uint8_t> held(cd.worldTransforms.size(),0);
    for(const auto &scene:cur.perspectiveScenes){
        if(scene.projections.size()!=1)continue;
        const auto *cp=projection(q,cw,scene.projections[0]);if(!cp)continue;
        const auto view=cp->transformsIndex;
        if(view>=cd.viewTransforms.size()||view>=cd.viewProjTransformGroups.size()||cd.viewProjTransformGroups[view]>=cd.transformGroups.size())continue;
        if(cd.transformGroups[cd.viewProjTransformGroups[view]].matrixId!=G_EX_ID_AUTO)continue;
        // AUTO cameras explicitly select INTERPOLATE in matchScene. Every
        // possible previous scene must have a changed view translation.
        bool allChanged=true;
        for(const auto &oldScene:prev.perspectiveScenes){
            if(oldScene.projections.size()!=1){allChanged=false;break;}
            const auto *op=projection(q,pw,oldScene.projections[0]);
            if(!op||op->transformsIndex>=pd.viewTransforms.size()||
                !translationDiffers(cd.viewTransforms[view],pd.viewTransforms[op->transformsIndex])){allChanged=false;break;}
        }
        if(!allChanged||cp->gameCallCount>cp->gameCalls.size())continue;
        for(unsigned c=0;c<cp->gameCallCount;++c){
            const auto &call=cp->gameCalls[c];uint64_t begin=call.meshDesc.faceIndicesStart,end=begin+uint64_t(call.callDesc.triangleCount)*3;
            if(end>cd.faceIndices.size())return false;
            for(auto i=begin;i<end;i+=3){
                uint32_t v[3]={cd.faceIndices[size_t(i)],cd.faceIndices[size_t(i+1)],cd.faceIndices[size_t(i+2)]};
                if(v[0]==v[1]||v[0]==v[2]||v[1]==v[2])continue;
                for(auto vertex:v){
                    if(vertex>=cd.worldIndices.size())return false;uint32_t world=cd.worldIndices[vertex];
                    if(world>=cd.worldTransforms.size())return false;
                    if(held[world]==2)return true;if(held[world]==1)continue;held[world]=1;
                    const auto &group=cd.transformGroups[cd.worldTransformGroups[world]];
                    auto id=ci.find(group.matrixId),old=pi.find(group.matrixId);
                    if(group.ordering!=G_EX_ORDER_LINEAR||id==ci.end()||id->second.count!=1||old==pi.end()||old->second.count!=1)continue;
                    uint32_t prior=old->second.index;
                    if(prev.matched){
                        // The full matcher trusts this existing map instead of
                        // rebuilding it. Defer if it differs from our witness.
                        const auto range=q.workloads[pw].transformIdMap.equal_range(group.matrixId);
                        if(range.first==range.second||range.first->second!=prior)continue;
                        auto next=range.first;++next;if(next!=range.second)continue;
                    }
                    if(!translationDiffers(cd.worldTransforms[world],pd.worldTransforms[prior]))continue;
                    RigidBody body;
                    if(history){if(prior>=history->transforms.size())return false;body=history->transforms[prior].rigidBody;}
                    body.updateLinear(pd.worldTransforms[prior],cd.worldTransforms[world],group.positionInterpolation);
                    if(!body.lerpTranslation){held[world]=2;return true;}
                }
            }
        }
    }
    return false;
}
}
