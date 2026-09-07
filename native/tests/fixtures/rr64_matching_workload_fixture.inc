static void seed(Workload &w, unsigned transforms, unsigned triangles, unsigned mode) {
    w.reset(); auto &d=w.drawData;
    d.transformGroups.clear(); d.worldTransformGroups.clear(); d.worldTransforms.clear(); d.worldTransformVertexIndices.clear();
    TransformGroup camera{}; d.transformGroups.push_back(camera); d.viewProjTransformGroups={0};
    auto identity=hlslpp::float4x4::identity(); d.viewTransforms={identity};d.projTransforms={identity};d.viewProjTransforms={identity};
    w.presentationScene={1,true}; w.fbPairCount=1;
    auto &pair=w.fbPairs[0];pair.projections.resize(mode==4?2:1);pair.projectionCount=unsigned(pair.projections.size());
    for(auto &p:pair.projections){p.reset();p.type=Projection::Type::Perspective;p.transformsIndex=0;}
    const unsigned vertices=triangles+2;
    for(unsigned t=0;t<transforms;++t) {
        TransformGroup group{};group.matrixId=(mode>=11&&mode<=14)?G_EX_ID_AUTO:0x52510000+t;group.ordering=(mode>=11&&mode<=14)?G_EX_ORDER_AUTO:G_EX_ORDER_LINEAR;
        d.transformGroups.push_back(group);d.worldTransformGroups.push_back(t+1);d.worldTransforms.push_back(identity);
        unsigned first=d.vertexCount();d.worldTransformVertexIndices.push_back(first);
        for(unsigned v=0;v<vertices;++v){d.posFloats.insert(d.posFloats.end(),{float(v),float(v%7),float(t%3)});d.worldIndices.push_back(uint16_t(t));d.tcFloats.insert(d.tcFloats.end(),{0,0});}
        if(mode==11||mode==13||mode==14)for(unsigned i=first*3+2;i<d.posFloats.size();i+=3)d.posFloats[i]=float(t);
        GameCall c{};c.callDesc.minWorldMatrix=c.callDesc.maxWorldMatrix=uint16_t(t);c.callDesc.triangleCount=triangles;c.meshDesc.faceIndicesStart=unsigned(d.faceIndices.size());
        for(unsigned i=0;i<triangles;++i)d.faceIndices.insert(d.faceIndices.end(),{first,first+i+1,first+i+2});
        pair.projections[0].addGameCall(c);
        if(mode==4&&t%2==0)pair.projections[1].addGameCall(c);
    }
    d.velFloats.resize(d.posFloats.size());d.tcVelFloats.resize(d.tcFloats.size());
    if(mode==1)d.worldIndices[0]=65535;
    if(mode==2)d.worldIndices[0]=1;
    if(mode==3)d.faceIndices[1]=d.faceIndices[0];
    if(mode==6)d.posFloats[0]=std::numeric_limits<float>::quiet_NaN();
    if(mode==8)d.posFloats[0]=std::numeric_limits<float>::infinity();
    if(mode==9)for(unsigned i=0;i<d.posFloats.size();i+=3)d.posFloats[i]+=float(i)*0.25f;
    if(mode==10)for(auto &p:pair.projections)for(auto &c:p.gameCalls)c.callDesc.triangleCount=0;
    if(mode==15)++d.worldTransformVertexIndices.back();
    if(mode==16){
        for(unsigned t=0;t<transforms;++t){auto& group=d.transformGroups[t+1];group.matrixId=G_EX_ID_AUTO;group.ordering=G_EX_ORDER_AUTO;}
        const auto calls=pair.projections[0].gameCalls;
        for(unsigned repeat=0;repeat<7;++repeat)for(const auto& c:calls)pair.projections[0].addGameCall(c);
    }
}
