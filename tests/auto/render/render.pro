TEMPLATE = subdirs

qtConfig(private_tests) {
    SUBDIRS += \
        entity \
        renderqueue \
        renderpass \
        qgraphicsutils \
        shader \
        texture \
        renderviewutils \
        renderviews \
        material \
        vsyncframeadvanceservice \
        meshfunctors \
        qmaterial \
        qattribute \
        qbuffer \
        qgeometry \
        qgeometryrenderer \
        qlevelofdetail \
        levelofdetail \
        buffer \
        attribute \
        geometry \
        geometryloaders \
        geometryrenderer \
        raycasting \
        qcameraselector \
        qclearbuffers \
        qframegraphnode \
        qlayerfilter \
        qabstractlight \
        qenvironmentlight \
        qray3d \
        qrenderpassfilter \
        qrendertargetselector \
        qsortpolicy \
        qrenderstateset \
        qtechniquefilter \
        qtextureimagedata \
        qviewport \
        framegraphnode \
        qobjectpicker \
        objectpicker \
        picking \
#        qboundingvolumedebug \
#        boundingvolumedebug \
        boundingsphere \
        qdefaultmeshes \
        trianglesextractor \
        triangleboundingvolume \
        ddstextures \
        shadercache \
        layerfiltering \
        filterentitybycomponent \
        genericlambdajob \
        qgraphicsapifilter \
        qrendersurfaceselector \
        sortpolicy \
        sceneloader \
        qsceneloader \
        qrendertargetoutput \
        qcameralens \
        qcomputecommand \
        loadscenejob \
        qrendercapture \
        uniform \
        graphicshelpergl3_3 \
        graphicshelpergl3_2 \
        graphicshelpergl2 \
        gltfplugins \
        pickboundingvolumejob \
        sendrendercapturejob \
        textures \
        qparameter \
        parameter \
        qtextureloader \
        qtextureimage \
        qabstracttexture \
        qabstracttextureimage \
        qrendersettings \
        updatemeshtrianglelistjob \
        updateshaderdatatransformjob \
        texturedatamanager \
        rendertarget \
        transform \
        computecommand \
        qrendertarget \
        qdispatchcompute \
        qtechnique \
        qeffect \
        qrenderpass \
        qfilterkey \
        effect \
        filterkey \
        qmesh \
        technique \
        materialparametergathererjob \
        renderviewbuilder \
        filtercompatibletechniquejob \
        rendercapture \
        segmentvisitor \
        trianglevisitor \
        qmemorybarrier \
        memorybarrier \
        qshaderprogram \
        qscene2d \
        scene2d \
        coordinatereader \
        framegraphvisitor

    !macos: SUBDIRS += graphicshelpergl4
}
