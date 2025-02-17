#ifndef BASICCLUSTERENDPOINT_H
#define BASICCLUSTERENDPOINT_H

#include "Endpoint.h"
#include "OTAHandlers.h"

extern "C"
{
    #include "zcl.h"
    #include "Basic.h"
    #include "Identify.h"
}

// List of cluster instances (descriptor objects) that are included into an Endpoint
struct BasicClusterInstances
{
    // All devices have basic mandatory clusters
    tsZCL_ClusterInstance sBasicServer;

    // Zigbee device may have also OTA optional clusters for the client
    tsZCL_ClusterInstance sOTAClient;

    // Identify cluster (whole device handler)
    tsZCL_ClusterInstance sIdentifyServer;
} __attribute__ ((aligned(4)));

class BasicClusterEndpoint : public Endpoint
{
    tsZCL_EndPointDefinition endPoint;

    // Cluster instances
    BasicClusterInstances clusterInstances;

    // Value storage for endpoint's clusters
    tsCLD_Basic sBasicServerCluster;
    tsCLD_Identify sIdentifyServerCluster;
    tsCLD_IdentifyCustomDataStructure sIdentifyClusterData;
    tsCLD_AS_Ota sOTAClientCluster;
    tsOTA_Common sOTACustomDataStruct;

    OTAHandlers otaHandlers;

public:
    BasicClusterEndpoint();

    virtual void init();

protected:
    virtual void registerBasicCluster();
    virtual void registerIdentifyCluster();
    virtual void registerOtaCluster();
    virtual void registerEndpoint();

    virtual void handleClusterUpdate(tsZCL_CallBackEvent *psEvent);
    virtual void handleCustomClusterEvent(tsZCL_CallBackEvent *psEvent);

    void handleIdentifyClusterEvent(tsZCL_CallBackEvent *psEvent);
    void handleIdentifyClusterUpdate(tsZCL_CallBackEvent *psEvent);
    void handleOTAClusterUpdate(tsZCL_CallBackEvent *psEvent);
};

#endif // BASICCLUSTERENDPOINT_H
