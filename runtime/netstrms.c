/* netstrms.c
 * 
 * Work on this module begung 2008-04-23 by Rainer Gerhards.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
//#include "errmsg.h"
//#include "net.h"
#include "nsd.h"
#include "netstrm.h"
#include "netstrms.h"

MODULE_TYPE_LIB

/* static data */
DEFobjStaticHelpers
//DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(netstrm)
//DEFobjCurrIf(net)


/* load our low-level driver. This must be done before any
 * driver-specific functions (allmost all...) can be carried
 * out. Note that the driver's .ifIsLoaded is correctly
 * initialized by calloc() and we depend on that.
 * rgerhards, 2008-04-18
 */
static rsRetVal
loadDrvr(netstrms_t *pThis)
{
	DEFiRet;
	uchar *pBaseDrvrName;
	uchar szDrvrName[48]; /* 48 shall be large enough */

	pBaseDrvrName = pThis->pDrvrName;
	if(pBaseDrvrName == NULL) /* if no drvr name is set, use system default */
		pBaseDrvrName = glbl.GetDfltNetstrmDrvr();
	if(snprintf((char*)szDrvrName, sizeof(szDrvrName), "lmnsd_%s", pBaseDrvrName) == sizeof(szDrvrName))
		ABORT_FINALIZE(RS_RET_DRVRNAME_TOO_LONG);

	pThis->Drvr.ifVersion = nsdCURR_IF_VERSION;
	/* The pDrvrName+2 below is a hack to obtain the object name. It 
	 * safes us to have yet another variable with the name without "lm" in
	 * front of it. If we change the module load interface, we may re-think
	 * about this hack, but for the time being it is efficient and clean
	 * enough. -- rgerhards, 2008-04-18
	 */
	CHKiRet(obj.UseObj(__FILE__, szDrvrName+2, szDrvrName, (void*) &pThis->Drvr));
finalize_it:
	RETiRet;
}


/* Standard-Constructor */
BEGINobjConstruct(netstrms) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(netstrms)


/* destructor for the netstrms object */
BEGINobjDestruct(netstrms) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(netstrms)
	if(pThis->pDrvrName != NULL)
		free(pThis->pDrvrName);
ENDobjDestruct(netstrms)


/* ConstructionFinalizer */
static rsRetVal
netstrmsConstructFinalize(netstrms_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, netstrms);
	CHKiRet(loadDrvr(pThis));
finalize_it:
	RETiRet;
}


/* create an instance of a netstrm object. It is initialized with default
 * values. The current driver is used. The caller may set netstrm properties
 * and must call ConstructFinalize().
 */
static rsRetVal
CreateStrm(netstrms_t *pThis, netstrm_t **ppStrm)
{
	netstrm_t *pStrm = NULL;
	DEFiRet;

	CHKiRet(objUse(netstrm, LM_NETSTRM_FILENAME));
	CHKiRet(netstrm.Construct(&pStrm));
	/* we copy over our driver structure. We could provide a pointer to 
	 * ourselves, but that costs some performance on each driver invocation.
	 * As we already have hefty indirection (and thus performance toll), I
	 * prefer to copy over the function pointers here. -- rgerhards, 2008-04-23
	 */
	memcpy(&pStrm->Drvr, &pThis->Drvr, sizeof(pThis->Drvr));
	pStrm->pNS = pThis;
	
	*ppStrm = pStrm;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pStrm != NULL)
			netstrm.Destruct(&pStrm);
	}
	RETiRet;
}


/* queryInterface function */
BEGINobjQueryInterface(netstrms)
CODESTARTobjQueryInterface(netstrms)
	if(pIf->ifVersion != netstrmsCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = netstrmsConstruct;
	pIf->ConstructFinalize = netstrmsConstructFinalize;
	pIf->Destruct = netstrmsDestruct;
	pIf->CreateStrm = CreateStrm;
finalize_it:
ENDobjQueryInterface(netstrms)


/* exit our class */
BEGINObjClassExit(netstrms, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(netstrms)
	/* release objects we no longer need */
	//objRelease(net, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(netstrm, LM_NETSTRM_FILENAME);
ENDObjClassExit(netstrms)


/* Initialize the netstrms class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(netstrms, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	//CHKiRet(objUse(net, CORE_COMPONENT));

	/* set our own handlers */
ENDObjClassInit(netstrms)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	netstrmsClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(netstrmsClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
/* vi:set ai:
 */
