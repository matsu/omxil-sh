/**
   src/vpu/shvpu_avcenc_omx.c

   This component implements H.264 / MPEG-4 AVC video encoder.
   The H.264 / MPEG-4 AVC Video encoder is implemented on the
   Renesas's VPU5HG middleware library.

   Copyright (C) 2010 IGEL Co., Ltd
   Copyright (C) 2010 Renesas Solutions Corp.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA

*/

#include <bellagio/omxcore.h>
#include <bellagio/omx_base_video_port.h>
#include "shvpu_avcenc_omx.h"
#include <OMX_Video.h>

/** Maximum Number of Video Component Instance*/
#define MAX_COMPONENT_VIDEOENC 1

/** Counter of Video Component Instance*/
static OMX_U32 noVideoEncInstance = 0;

#define DEFAULT_WIDTH 128
#define DEFAULT_HEIGHT 96
/** define the minimum input buffer size */
#define DEFAULT_VIDEO_INPUT_BUF_SIZE					\
	(DEFAULT_WIDTH * DEFAULT_HEIGHT * 3 / 2)	// YUV subQCIF
#define DEFAULT_VIDEO_OUTPUT_BUF_SIZE	256

#define INPUT_BUFFER_COUNT 2
#define INPUT_PICTURE_COLOR_FMT		OMX_COLOR_FormatYUV420Planar

#undef DEBUG_LEVEL
#define DEBUG_LEVEL DEB_ALL_MESS
static void *
shvpu_avcenc_BufferMgmtFunction(void *param);

/** The Constructor of the video encoder component
 * @param pComponent the component handle to be constructed
 * @param cComponentName is the name of the constructed component
 */
OMX_ERRORTYPE
shvpu_avcenc_Constructor(OMX_COMPONENTTYPE * pComponent,
			 OMX_STRING cComponentName)
{

	OMX_ERRORTYPE eError = OMX_ErrorNone;
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private;
	omx_base_video_PortType *inPort, *outPort;
	OMX_U32 i;

	/*
	 * initialize component private data
	 */
	if (!pComponent->pComponentPrivate) {
		DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n",
		      __func__);
		pComponent->pComponentPrivate =
			calloc(1, sizeof(shvpu_avcenc_PrivateType));
		if (pComponent->pComponentPrivate == NULL) {
			return OMX_ErrorInsufficientResources;
		}
	} else {
		DEBUG(DEB_LEV_FUNCTION_NAME,
		      "In %s, Error Component %x Already Allocated\n",
		      __func__, (int)pComponent->pComponentPrivate);
	}

	shvpu_avcenc_Private = pComponent->pComponentPrivate;
	shvpu_avcenc_Private->ports = NULL;

	/*
	 * construct base filter
	 */
	eError = omx_base_filter_Constructor(pComponent, cComponentName);

	shvpu_avcenc_Private->
		sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
	shvpu_avcenc_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

	/*
	 * Allocate Ports and call port constructor.
	 */
	if (shvpu_avcenc_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts
	    && !shvpu_avcenc_Private->ports) {
		shvpu_avcenc_Private->ports =
			calloc(shvpu_avcenc_Private->sPortTypesParam
			       [OMX_PortDomainVideo].nPorts,
			       sizeof(omx_base_PortType *));
		if (!shvpu_avcenc_Private->ports) {
			return OMX_ErrorInsufficientResources;
		}
		for (i = 0;
		     i <shvpu_avcenc_Private->sPortTypesParam
			     [OMX_PortDomainVideo].nPorts; i++) {
			shvpu_avcenc_Private->ports[i] =
				calloc(1, sizeof(omx_base_video_PortType));
			if (!shvpu_avcenc_Private->ports[i]) {
				return OMX_ErrorInsufficientResources;
			}
		}
	}

	base_video_port_Constructor(pComponent,
				    &shvpu_avcenc_Private->ports[0], 0,
				    OMX_TRUE);
	base_video_port_Constructor(pComponent,
				    &shvpu_avcenc_Private->ports[1], 1,
				    OMX_FALSE);

	/*
	 * set the parameter common to input/output ports
	 */
	inPort = (omx_base_video_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	inPort->sPortParam.format.video.nFrameWidth = DEFAULT_WIDTH;
	inPort->sPortParam.format.video.nFrameHeight = DEFAULT_HEIGHT;
	inPort->sPortParam.nBufferSize = DEFAULT_VIDEO_INPUT_BUF_SIZE;
	inPort->sPortParam.nBufferCountMin = INPUT_BUFFER_COUNT;
	inPort->sPortParam.nBufferCountActual = INPUT_BUFFER_COUNT;
	inPort->sPortParam.format.video.xFramerate = 0;
	inPort->sPortParam.format.video.eColorFormat =
		INPUT_PICTURE_COLOR_FMT;
	inPort->sPortParam.format.video.eCompressionFormat =
		OMX_VIDEO_CodingAVC;

	inPort->sVideoParam.eColorFormat = INPUT_PICTURE_COLOR_FMT;

	outPort = (omx_base_video_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	outPort->sPortParam.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUF_SIZE;
	outPort->sPortParam.format.video.xFramerate = 0;

	SetInternalVideoParameters(pComponent);

	/*
	 * general configuration irrespective of any video formats
	 * setting other parameters of shvpu_avcenc_private
	 */
	shvpu_avcenc_Private->avcodecReady = OMX_FALSE;

	/** initializing the codec context etc that was done earlier
	    by vpulibinit function */
	shvpu_avcenc_Private->BufferMgmtFunction =
		shvpu_avcenc_BufferMgmtFunction;
	shvpu_avcenc_Private->messageHandler = shvpu_avcenc_MessageHandler;
	shvpu_avcenc_Private->destructor = shvpu_avcenc_Destructor;
	pComponent->SetParameter = shvpu_avcenc_SetParameter;
	pComponent->GetParameter = shvpu_avcenc_GetParameter;
	pComponent->ComponentRoleEnum = shvpu_avcenc_ComponentRoleEnum;
	//pComponent->GetExtensionIndex = shvpu_avcenc_GetExtensionIndex;

	noVideoEncInstance++;

	if (noVideoEncInstance > MAX_COMPONENT_VIDEOENC) {
		return OMX_ErrorInsufficientResources;
	}

	return eError;
}

/** The destructor of the video encoder component
 */
OMX_ERRORTYPE
shvpu_avcenc_Destructor(OMX_COMPONENTTYPE * pComponent)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		pComponent->pComponentPrivate;
	OMX_U32 i;

	omx_base_filter_Destructor(pComponent);
	noVideoEncInstance--;

	return OMX_ErrorNone;
}

/** It initializates the VPU framework, and opens an VPU videodecoder
    of type specified by IL client
*/
OMX_ERRORTYPE
shvpu_avcenc_vpuLibInit(shvpu_avcenc_PrivateType * shvpu_avcenc_Private)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "VPU library/codec initialized\n");
	return OMX_ErrorNone;
}

/** It Deinitializates the vpu framework, and close the vpu video
    decoder of selected coding type
*/
void
shvpu_avcenc_vpuLibDeInit(shvpu_avcenc_PrivateType *
			  shvpu_avcenc_Private)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "VPU library/codec de-initialized\n");
}

/** The Initialization function of the video encoder
 */
OMX_ERRORTYPE
shvpu_avcenc_Init(OMX_COMPONENTTYPE * pComponent)
{

	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		pComponent->pComponentPrivate;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	/** Temporary First Output buffer size */
	shvpu_avcenc_Private->isFirstBuffer = OMX_TRUE;
	shvpu_avcenc_Private->isNewBuffer = 1;

	return eError;
}

/** The Deinitialization function of the video encoder
 */
OMX_ERRORTYPE
shvpu_avcenc_Deinit(OMX_COMPONENTTYPE * pComponent)
{

	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		pComponent->pComponentPrivate;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	if (shvpu_avcenc_Private->avcodecReady) {
		shvpu_avcenc_vpuLibDeInit(shvpu_avcenc_Private);
		shvpu_avcenc_Private->avcodecReady = OMX_FALSE;
	}

	return eError;
}

static inline OMX_ERRORTYPE
handleState_IdletoExecuting(OMX_COMPONENTTYPE * pComponent)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		(shvpu_avcenc_PrivateType *) pComponent->pComponentPrivate;

	shvpu_avcenc_Private->isFirstBuffer = OMX_TRUE;
	return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE
handleState_IdletoLoaded(OMX_COMPONENTTYPE * pComponent)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		(shvpu_avcenc_PrivateType *) pComponent->pComponentPrivate;
	OMX_ERRORTYPE err;

	err = shvpu_avcenc_Deinit(pComponent);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR,
		      "In %s Video Encoder DeinitFailed Error=%x\n",
		      __func__, err);
	}

	return err;
}

static inline OMX_ERRORTYPE
handleState_LoadedtoIdle(OMX_COMPONENTTYPE * pComponent,
			internalRequestMessageType * message)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		(shvpu_avcenc_PrivateType *) pComponent->pComponentPrivate;
	OMX_ERRORTYPE err;

	err = omx_base_component_MessageHandler(pComponent, message);
	if (err != OMX_ErrorNone)
		return err;

	err = shvpu_avcenc_vpuLibInit(shvpu_avcenc_Private);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR,
		      "In %s shvpu_avcenc_vpuLibInit Failed\n",
		      __func__);
		return err;
	}

	err = shvpu_avcenc_Init(pComponent);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR,
		      "In %s Video Encoder Init"
		      "Failed Error=%x\n",
		      __func__, err);
		return err;
	}

	shvpu_avcenc_Private->avcodecReady = OMX_TRUE;

	return err;
}

OMX_ERRORTYPE
shvpu_avcenc_MessageHandler(OMX_COMPONENTTYPE * pComponent,
			    internalRequestMessageType * message)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		(shvpu_avcenc_PrivateType *) pComponent->pComponentPrivate;
	OMX_ERRORTYPE err;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

	if (message->messageType == OMX_CommandStateSet) {
		if ((shvpu_avcenc_Private->state == OMX_StateIdle) &&
		    (message->messageParam == OMX_StateExecuting)) {
			err = handleState_IdletoExecuting(pComponent);
		} else if ((shvpu_avcenc_Private->state == OMX_StateIdle) &&
			   (message->messageParam == OMX_StateLoaded)) {
			err = handleState_IdletoLoaded(pComponent);
			if (err != OMX_ErrorNone)
				return err;
		} if ((shvpu_avcenc_Private->state == OMX_StateLoaded) &&
		      (message->messageParam == OMX_StateIdle)) {
			err = handleState_LoadedtoIdle(pComponent, message);
			if (err != OMX_ErrorNone)
				return err;
		}
	} else if ((message->messageType == OMX_FreeHandle) &&
		   (shvpu_avcenc_Private->state != OMX_StateLoaded)) {
		err = shvpu_avcenc_Deinit(pComponent);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,
			      "In %s Video Encoder Deinit"
			      "Failed Error=%x\n",
			      __func__, err);
			return err;
		}
	}

	// Execute the base message handling
	err = omx_base_component_MessageHandler(pComponent, message);

	return err;
}

/** Executes all the required steps after an input buffer
    frame-size has changed.
*/
static inline void
UpdateFrameSize(OMX_COMPONENTTYPE *pComponent) {
	  shvpu_avcenc_PrivateType* shvpu_avcenc_Private =
		  pComponent->pComponentPrivate;
	  omx_base_video_PortType *inPort =
		  (omx_base_video_PortType *)
		  shvpu_avcenc_Private->
		  ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

	  switch(inPort->sPortParam.format.video.eColorFormat) {
	  case OMX_COLOR_FormatYUV420Planar:
		        inPort->sPortParam.nBufferSize =
				inPort->sPortParam.format.video.nFrameWidth *
				inPort->sPortParam.format.video.nFrameHeight *
				3/2;
			break;
	  default:
		        inPort->sPortParam.nBufferSize =
				inPort->sPortParam.format.video.nFrameWidth *
				inPort->sPortParam.format.video.nFrameHeight *
				2;
			break;
	  }
}

OMX_ERRORTYPE
shvpu_avcenc_SetParameter(OMX_HANDLETYPE hComponent,
			  OMX_INDEXTYPE nParamIndex,
			  OMX_PTR ComponentParameterStructure)
{

	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 portIndex;

	/* Check which structure we are being fed and
	   make control its header */
	OMX_COMPONENTTYPE *pComponent = hComponent;
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		pComponent->pComponentPrivate;
	omx_base_video_PortType *port;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch (nParamIndex) {
	case OMX_IndexParamPortDefinition:
	{
		eError = omx_base_component_SetParameter
			(hComponent, nParamIndex,
			 ComponentParameterStructure);
		if (eError == OMX_ErrorNone) {
			OMX_PARAM_PORTDEFINITIONTYPE *pPortDef =
				(OMX_PARAM_PORTDEFINITIONTYPE *)
				ComponentParameterStructure;
			UpdateFrameSize(pComponent);
			portIndex = pPortDef->nPortIndex;
			port = (omx_base_video_PortType *)
				shvpu_avcenc_Private->ports[portIndex];
			port->sVideoParam.eColorFormat =
				port->sPortParam.format.video.
				eColorFormat;
		}
		break;
	}
	case OMX_IndexParamStandardComponentRole:
	{
		OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
		pComponentRole = ComponentParameterStructure;
		if (shvpu_avcenc_Private->state != OMX_StateLoaded
		    && shvpu_avcenc_Private->state !=
		    OMX_StateWaitForResources) {
			DEBUG(DEB_LEV_ERR,
			      "In %s Incorrect State=%x lineno=%d\n",
			      __func__, shvpu_avcenc_Private->state,
			      __LINE__);
			return OMX_ErrorIncorrectStateOperation;
		}

		if ((eError =
		     checkHeader(ComponentParameterStructure,
				 sizeof(OMX_PARAM_COMPONENTROLETYPE)))
		    != OMX_ErrorNone) {
			break;
		}

		if (strcmp((char *)pComponentRole->cRole,
			    VIDEO_ENC_H264_ROLE))
			return OMX_ErrorBadParameter;
		SetInternalVideoParameters(pComponent);
		break;
	}
	default:		/*Call the base component function */
		eError = omx_base_component_SetParameter
			(hComponent, nParamIndex,
			 ComponentParameterStructure);
	}
	return eError;
}

OMX_ERRORTYPE
shvpu_avcenc_GetParameter(OMX_HANDLETYPE hComponent,
			  OMX_INDEXTYPE nParamIndex,
			  OMX_PTR ComponentParameterStructure)
{

	omx_base_video_PortType *port;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	OMX_COMPONENTTYPE *pComponent = hComponent;
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		pComponent->pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch (nParamIndex) {
	case OMX_IndexParamStandardComponentRole:
	{
		OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
		pComponentRole = ComponentParameterStructure;
		if ((eError =
		     checkHeader(ComponentParameterStructure,
				 sizeof(OMX_PARAM_COMPONENTROLETYPE)))
		    != OMX_ErrorNone) {
			break;
		}
		strcpy((char *)pComponentRole->cRole, VIDEO_ENC_H264_ROLE);
		break;
	}
	default:		/*Call the base component function */
		eError = omx_base_component_GetParameter
			(hComponent, nParamIndex,
			 ComponentParameterStructure);
	}
	return eError;
}

OMX_ERRORTYPE
shvpu_avcenc_ComponentRoleEnum(OMX_HANDLETYPE hComponent,
			       OMX_U8 * cRole, OMX_U32 nIndex)
{

	if (nIndex == 0) {
		strcpy((char *)cRole, VIDEO_ENC_H264_ROLE);
	} else {
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}

static inline void
handle_buffer_flush(shvpu_avcenc_PrivateType *shvpu_avcenc_Private,
		    OMX_BOOL *pIsInBufferNeeded,
		    OMX_BOOL *pIsOutBufferNeeded,
		    int *pInBufExchanged, int *pOutBufExchanged,
		    OMX_BUFFERHEADERTYPE **ppInBuffer,
		    OMX_BUFFERHEADERTYPE **ppOutBuffer)
{
	omx_base_PortType *pInPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	omx_base_PortType *pOutPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	tsem_t *pInputSem = pInPort->pBufferSem;
	tsem_t *pOutputSem = pOutPort->pBufferSem;
	int i;

	pthread_mutex_lock(&shvpu_avcenc_Private->flush_mutex);
	while (PORT_IS_BEING_FLUSHED(pInPort) ||
	       PORT_IS_BEING_FLUSHED(pOutPort)) {
		pthread_mutex_unlock
			(&shvpu_avcenc_Private->flush_mutex);

		DEBUG(DEB_LEV_FULL_SEQ,
		      "In %s 1 signalling flush all cond iE=%d,"
		      "iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n",
		      __func__, *pInBufExchanged, *pIsInBufferNeeded,
		      *pOutBufExchanged, *pIsOutBufferNeeded,
		      pInputSem->semval, pOutputSem->semval);

		if (*pIsOutBufferNeeded == OMX_FALSE
		    && PORT_IS_BEING_FLUSHED(pOutPort)) {
			pOutPort->ReturnBufferFunction(pOutPort,
						       *ppOutBuffer);
			(*pOutBufExchanged)--;
			*ppOutBuffer = NULL;
			*pIsOutBufferNeeded = OMX_TRUE;
			DEBUG(DEB_LEV_FULL_SEQ,
			      "Ports are flushing,so returning "
			      "output buffer\n");
		}

		if (*pIsInBufferNeeded == OMX_FALSE
		    && PORT_IS_BEING_FLUSHED(pInPort)) {
			pInPort->ReturnBufferFunction(pInPort, *ppInBuffer);
			*ppInBuffer = NULL;
			(*pInBufExchanged)--;
			*pIsInBufferNeeded = OMX_TRUE;
			DEBUG(DEB_LEV_FULL_SEQ,
			      "Ports are flushing,so returning "
			      "input buffer\n");
		}

		DEBUG(DEB_LEV_FULL_SEQ,
		      "In %s 2 signalling flush all cond iE=%d,"
		      "iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n",
		      __func__, *pInBufExchanged, *pIsInBufferNeeded,
		      *pOutBufExchanged, *pIsOutBufferNeeded,
		      pInputSem->semval, pOutputSem->semval);

		tsem_up(shvpu_avcenc_Private->flush_all_condition);
		tsem_down(shvpu_avcenc_Private->flush_condition);
		pthread_mutex_lock(&shvpu_avcenc_Private->
				   flush_mutex);
	}
	pthread_mutex_unlock(&shvpu_avcenc_Private->flush_mutex);

	return;
}

static inline int
waitBuffers(shvpu_avcenc_PrivateType *shvpu_avcenc_Private,
	     OMX_BOOL isInBufferNeeded, OMX_BOOL isOutBufferNeeded)
{
	omx_base_PortType *pInPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	omx_base_PortType *pOutPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	tsem_t *pInputSem = pInPort->pBufferSem;
	tsem_t *pOutputSem = pOutPort->pBufferSem;

	if ((isInBufferNeeded == OMX_TRUE &&
	     pInputSem->semval == 0) &&
	    (shvpu_avcenc_Private->state != OMX_StateLoaded &&
	     shvpu_avcenc_Private->state != OMX_StateInvalid)) {
		//Signalled from EmptyThisBuffer or
		//FillThisBuffer or some thing else
		DEBUG(DEB_LEV_FULL_SEQ,
		      "Waiting for next input/output buffer\n");
		tsem_down(shvpu_avcenc_Private->bMgmtSem);

	}

	if (shvpu_avcenc_Private->state == OMX_StateLoaded
	    || shvpu_avcenc_Private->state == OMX_StateInvalid) {
		DEBUG(DEB_LEV_SIMPLE_SEQ,
		      "In %s Buffer Management Thread is exiting\n",
		      __func__);
		return -1;
	}

	if ((isOutBufferNeeded == OMX_TRUE &&
	     pOutputSem->semval == 0) &&
	    (shvpu_avcenc_Private->state != OMX_StateLoaded &&
	     shvpu_avcenc_Private->state != OMX_StateInvalid) &&
	    !(PORT_IS_BEING_FLUSHED(pInPort) ||
	      PORT_IS_BEING_FLUSHED(pOutPort))) {
		//Signalled from EmptyThisBuffer or
		//FillThisBuffer or some thing else
		DEBUG(DEB_LEV_FULL_SEQ,
		      "Waiting for next input/output buffer\n");
		tsem_down(shvpu_avcenc_Private->bMgmtSem);

	}

	if (shvpu_avcenc_Private->state == OMX_StateLoaded ||
	    shvpu_avcenc_Private->state == OMX_StateInvalid) {
		DEBUG(DEB_LEV_SIMPLE_SEQ,
		      "In %s Buffer Management Thread is exiting\n",
		      __func__);
		return -1;
	}

	return 0;
}

static inline OMX_BOOL
getInBuffer(shvpu_avcenc_PrivateType *shvpu_avcenc_Private,
	    OMX_BUFFERHEADERTYPE **ppInBuffer,
	    int *pInBufExchanged, queue_t *pProcessInBufQueue)
{
	omx_base_PortType *pInPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	tsem_t *pInputSem = pInPort->pBufferSem;
	queue_t *pInputQueue = pInPort->pBufferQueue;

	DEBUG(DEB_LEV_FULL_SEQ,
	      "Waiting for input buffer semval=%d \n", pInputSem->semval);

	tsem_down(pInputSem);
	if (pInputQueue->nelem == 0) {
		DEBUG(DEB_LEV_ERR, "inconsistent semaphore\n");
		return OMX_TRUE;
	}

	*ppInBuffer = dequeue(pInputQueue);
	if (*ppInBuffer == NULL) {
		DEBUG(DEB_LEV_ERR, "Had NULL input buffer!!\n");
		return OMX_TRUE;
	}
	if (((*ppInBuffer)->hMarkTargetComponent != NULL) ||
	    ((*ppInBuffer)->pMarkData != NULL) ||
	    ((*ppInBuffer)->nTimeStamp != 0) ||
	    ((*ppInBuffer)->nFlags != 0)) {
		loge("%s: hMarkTargetComponent = %p\n",
		     __FUNCTION__, (*ppInBuffer)->hMarkTargetComponent);
		loge("%s: pMarkData = %p\n", __FUNCTION__,
		     (*ppInBuffer)->pMarkData);
		loge("%s: nTimeStamp = %d\n", __FUNCTION__,
		     (*ppInBuffer)->nTimeStamp);
		loge("%s: nFlags = %08x\n", __FUNCTION__,
		     (*ppInBuffer)->nFlags);
	}

	(*pInBufExchanged)++;
	queue(pProcessInBufQueue, *ppInBuffer);

	return OMX_FALSE;
}

static inline OMX_BOOL
takeOutBuffer(shvpu_avcenc_PrivateType *shvpu_avcenc_Private,
	      OMX_BUFFERHEADERTYPE **ppOutBuffer,
	      int *pOutBufExchanged)
{
	omx_base_PortType *pOutPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	tsem_t *pOutputSem = pOutPort->pBufferSem;
	queue_t *pOutputQueue = pOutPort->pBufferQueue;

	tsem_down(pOutputSem);
	if (pOutputQueue->nelem == 0) {
		DEBUG(DEB_LEV_ERR,
		      "Had NULL output buffer!! op is=%d,iq=%d\n",
		      pOutputSem->semval, pOutputQueue->nelem);
		return OMX_TRUE;
	}

	*ppOutBuffer = dequeue(pOutputQueue);
	if (*ppOutBuffer == NULL) {
		DEBUG(DEB_LEV_ERR,
		      "Had NULL output buffer!! op is=%d,iq=%d\n",
		      pOutputSem->semval, pOutputQueue->nelem);
		return OMX_TRUE;
	}
	(*pOutBufExchanged)++;

	return OMX_FALSE;
}

static inline void
handleEventMark(OMX_COMPONENTTYPE *pComponent,
	     OMX_BUFFERHEADERTYPE *pInBuffer)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		(shvpu_avcenc_PrivateType *) pComponent->pComponentPrivate;

	if ((OMX_COMPONENTTYPE *)pInBuffer->hMarkTargetComponent ==
	    (OMX_COMPONENTTYPE *)pComponent) {
		/*Clear the mark and generate an event */
		(*(shvpu_avcenc_Private->callbacks->EventHandler))
			(pComponent, shvpu_avcenc_Private->callbackData,
			 OMX_EventMark,	/* The command was completed */
			 1,		/* The commands was a
				   	   OMX_CommandStateSet */
			 0,		/* The state has been changed
					   in message->messageParam2 */
			 pInBuffer->pMarkData);
	} else {
		/*If this is not the target component then pass the mark */
		shvpu_avcenc_Private->pMark.hMarkTargetComponent =
			pInBuffer->hMarkTargetComponent;
		shvpu_avcenc_Private->pMark.pMarkData =
			pInBuffer->pMarkData;
	}
	pInBuffer->hMarkTargetComponent = NULL;

	return;
}

static inline void
checkFillDone(OMX_COMPONENTTYPE * pComponent,
		OMX_BUFFERHEADERTYPE **ppOutBuffer,
		int *pOutBufExchanged,
		OMX_BOOL *pIsOutBufferNeeded)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		pComponent->pComponentPrivate;
	omx_base_PortType *pOutPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

	/*If EOS and Input buffer Filled Len Zero
	  then Return output buffer immediately */
	if (((*ppOutBuffer)->nFilledLen == 0) &&
	    !((*ppOutBuffer)->nFlags & OMX_BUFFERFLAG_EOS))
		return;

	if ((*ppOutBuffer)->nFlags & OMX_BUFFERFLAG_EOS) {
	        (*(shvpu_avcenc_Private->callbacks->EventHandler))
			(pComponent,
			 shvpu_avcenc_Private->callbackData,
			 OMX_EventBufferFlag, /* The command was completed */
			 1, /* The commands was a OMX_CommandStateSet */
			 (*ppOutBuffer)->nFlags,
			 /* The state has been changed
			    in message->messageParam2 */
			 NULL);
	}
	pOutPort->ReturnBufferFunction(pOutPort, *ppOutBuffer);
	(*pOutBufExchanged)--;
	*ppOutBuffer = NULL;
	*pIsOutBufferNeeded = OMX_TRUE;
}

static inline void
checkEmptyDone(shvpu_avcenc_PrivateType *shvpu_avcenc_Private,
	       queue_t *pInBufQueue, int *pInBufExchanged)
{
	omx_base_PortType *pInPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	OMX_BUFFERHEADERTYPE *pInBuffer;
	int n;

	/* Input Buffer has been completely consumed.
	  So,return input buffer */
	for (n = *pInBufExchanged; n > 0; n--) {
		pInBuffer = dequeue(pInBufQueue);
		if (pInBuffer->nFilledLen > 0) {
			queue(pInBufQueue, pInBuffer);
			continue;
		}

		pInPort->ReturnBufferFunction(pInPort, pInBuffer);
		(*pInBufExchanged)--;
	}
}

/** This function is used to process the input buffer and
    provide one output buffer
*/
void
shvpu_avcenc_EncodePicture(OMX_COMPONENTTYPE * pComponent,
			   OMX_BUFFERHEADERTYPE * pInBuffer,
			   OMX_BUFFERHEADERTYPE * pOutBuffer)
{
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private;

	shvpu_avcenc_Private = pComponent->pComponentPrivate;
	pOutBuffer->nFilledLen =
		(256 < pOutBuffer->nAllocLen) ? 256 : pOutBuffer->nAllocLen;
	pInBuffer->nFilledLen = 0;

	return;
}

/** This is the central function for component processing. It
 * is executed in a separate thread, is synchronized with
 * semaphores at each port, those are released each time a new buffer
 * is available on the given port.
 */
static void *
shvpu_avcenc_BufferMgmtFunction(void *param)
{
	OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *) param;
	shvpu_avcenc_PrivateType *shvpu_avcenc_Private =
		(shvpu_avcenc_PrivateType *) pComponent->pComponentPrivate;
	omx_base_PortType *pInPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	omx_base_PortType *pOutPort =
		(omx_base_PortType *)
		shvpu_avcenc_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
 	tsem_t *pInputSem = pInPort->pBufferSem;
	tsem_t *pOutputSem = pOutPort->pBufferSem;
	queue_t *pInputQueue = pInPort->pBufferQueue;
	queue_t *pOutputQueue = pOutPort->pBufferQueue;
	OMX_BUFFERHEADERTYPE *pOutBuffer = NULL;
	OMX_BUFFERHEADERTYPE *pInBuffer = NULL;
	OMX_BOOL isInBufferNeeded = OMX_TRUE,
		isOutBufferNeeded = OMX_TRUE;
	int inBufExchanged = 0, outBufExchanged = 0;
	queue_t processInBufQueue;
	int ret;

	shvpu_avcenc_Private->bellagioThreads->nThreadBufferMngtID =
		(long int)syscall(__NR_gettid);
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s of component %x\n", __func__,
	      (int)pComponent);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s the thread ID is %i\n", __func__,
	      (int)shvpu_avcenc_Private->bellagioThreads->
	      nThreadBufferMngtID);

	/* initialize a queue to keep processing input buffers */  
	queue_init(&processInBufQueue);

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	while (shvpu_avcenc_Private->state == OMX_StateIdle
	       || shvpu_avcenc_Private->state == OMX_StateExecuting
	       || shvpu_avcenc_Private->state == OMX_StatePause
	       || shvpu_avcenc_Private->transientState ==
	       OMX_TransStateLoadedToIdle) {

		/*Wait till the ports are being flushed */
		handle_buffer_flush(shvpu_avcenc_Private,
				    &isInBufferNeeded,
				    &isOutBufferNeeded,
				    &inBufExchanged, &outBufExchanged,
				    &pInBuffer, &pOutBuffer);

		/*No buffer to process. So wait here */
		ret = waitBuffers(shvpu_avcenc_Private,
				   isInBufferNeeded,
				   isOutBufferNeeded);
		if (ret < 0)
			break;

		if ((isInBufferNeeded == OMX_TRUE) &&
		    (pInputSem->semval > 0)) {
			getInBuffer(shvpu_avcenc_Private,
				    &pInBuffer,
				    &inBufExchanged, &processInBufQueue);
		}

		/* invoke a mark event callback if required */
		if (pInBuffer &&
		    (pInBuffer->hMarkTargetComponent != NULL))
			handleEventMark(pComponent, pInBuffer);

		/* get an output buffer */
		if ((isOutBufferNeeded == OMX_TRUE) &&
		    (pOutputSem->semval > 0))
			isOutBufferNeeded =
				takeOutBuffer(shvpu_avcenc_Private,
					      &pOutBuffer,
					      &outBufExchanged);

		/* do encoding if a pair of
		   input and output buffers are ensured */
		if (shvpu_avcenc_Private->state == OMX_StateExecuting) {
			if (pInBuffer && (pInBuffer->nFilledLen > 0))
				shvpu_avcenc_EncodePicture(pComponent,
							   pInBuffer,
							   pOutBuffer);
		} else if (!(PORT_IS_BEING_FLUSHED(pInPort) ||
			     PORT_IS_BEING_FLUSHED(pOutPort))) {
			DEBUG(DEB_LEV_ERR,
			      "In %s Received Buffer in non-"
			      "Executing State(%x)\n",
			      __func__,
			      (int)shvpu_avcenc_Private->state);
		}

		checkFillDone(pComponent,
				&pOutBuffer,
				&outBufExchanged,
				&isOutBufferNeeded);

		if (shvpu_avcenc_Private->state == OMX_StatePause
		    && !(PORT_IS_BEING_FLUSHED(pInPort)
			 || PORT_IS_BEING_FLUSHED(pOutPort))) {
			/*Waiting at paused state */
			tsem_wait(shvpu_avcenc_Private->bStateSem);
		}

		if (inBufExchanged > 0)
			checkEmptyDone(shvpu_avcenc_Private,
				       &processInBufQueue,
				       &inBufExchanged);

	}

	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s of component %x\n", __func__,
	      (int)pComponent);

	return NULL;
}
