/*
 * tclvalue.c --
 */

//#include "tclvalue.h"
#include <tcl.h>
#define PACKAGE_VERSION "0.1"
#define PACKAGE_NAME  "tclvalue"
#include <string.h>
#include <stdlib.h>

/* Copy of this, because it is not exported (but uses only public functionality) */
/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's internal
 * representation. Does not actually reset the rep's bytes. The ANSI C
 * "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclFreeIntRep(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclFreeIntRep(objPtr) \
	if ((objPtr)->typePtr != NULL) { \
		if ((objPtr)->typePtr->freeIntRepProc != NULL) { \
			(objPtr)->typePtr->freeIntRepProc(objPtr); \
		} \
		(objPtr)->typePtr = NULL; \
    }


/*
 * Functions handling the Tcl_ObjType 
 */

static int		SetTcl_ObjTypeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
/* static void		UpdateStringOfTclValue(Tcl_Obj *listPtr); 
 * this is a read-only type, similar to bytecode or regexp */

Tcl_ObjType Tcl_ObjTypeType = {
    "Tcl_ObjType",		/* name */
    NULL,			/* freeIntRepProc */
    NULL,			/* dupIntRepProc */
    NULL,			/* updateStringProc */
    SetTcl_ObjTypeFromAny		/* setFromAnyProc */
};

static void		DupTclValueInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void		FreeTclValueInternalRep(Tcl_Obj *listPtr);
static void		UpdateStringOfTclValue(Tcl_Obj *listPtr);


#ifdef LIST_INJECT
static int		SetListFromTclValue(Tcl_Interp *interp, Tcl_Obj *objPtr);
#endif

const Tcl_ObjType TclValueTclType_template = {
	"TclValue",			/* name */\
	FreeTclValueInternalRep,	/* freeIntRepProc */
	DupTclValueInternalRep,	/* dupIntRepProc */
	UpdateStringOfTclValue,	/* updateStringProc */
	NULL		/* setFromAnyProc */
	/* the conversion is done from Tcl code only
	 * therefore there is no way to construct these types from C code
	 */
};

/* derive from Tcl_ObjType to attach a bunch of Tcl_Obj* */
typedef struct {
	Tcl_ObjType objType;
	Tcl_Interp *masterInterp;
	Tcl_Interp *slaveInterp;
	Tcl_Obj* typeCmd;
	Tcl_Obj* constructor;
	Tcl_Obj* destructor;
	Tcl_Obj* clonemethod;
	Tcl_Obj* reprmethod;
} TclValueType;

/* Interpretation of the intRep in a TclValueType 
 * twoPtrValue.ptr1 = object command in the slave interp
 * twoPtrValue.ptr2 = alias in the master interp
 */

#define SlaveObjCommand(objPtr) (objPtr -> internalRep.twoPtrValue.ptr1)
#define MasterObjCommand(objPtr) (objPtr -> internalRep.twoPtrValue.ptr2)

static int SetTcl_ObjTypeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
	
	if (objPtr->typePtr == &Tcl_ObjTypeType) {
		return TCL_OK;
	}
	
	Tcl_ObjType *type =  Tcl_GetObjType(Tcl_GetStringFromObj(objPtr, NULL));
	
	if (type) {
		TclFreeIntRep(objPtr);
		objPtr -> internalRep.ptrAndLongRep.ptr = type;
		/* see if this is a type from our types by checking that the 
		 * free proc refers to our destructor for types */
		if (type -> freeIntRepProc == FreeTclValueInternalRep) {
			objPtr -> internalRep.ptrAndLongRep.value = 1;
		} else {
			objPtr -> internalRep.ptrAndLongRep.value = 0;
		}
		objPtr -> typePtr = &Tcl_ObjTypeType;
		return TCL_OK;
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("Tcl_ObjType not found", -1));
		return TCL_ERROR;
	}
}

/* utility function for invoking an obj command */

static int invoke(Tcl_Interp *interp, Tcl_Obj *obj1, Tcl_Obj *obj2, Tcl_Obj *obj3) {
	Tcl_Obj* cmdparts[3];
	cmdparts[0]=obj1;
	cmdparts[1]=obj2;
	cmdparts[2]=obj3;

	if (obj1 == NULL) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj("Can't invoke NULL", -1));
		return TCL_ERROR;
	}

	int objc=3;
	if (obj3 == NULL) {
		objc = 2;
	}

	if (obj2 == NULL) {
		objc = 1;
	}

	for (int n=0; n<objc; n++) Tcl_IncrRefCount(cmdparts[n]);

	int code = Tcl_EvalObjv(interp, objc, cmdparts, TCL_EVAL_GLOBAL);

	for (int n=0; n<objc; n++) Tcl_DecrRefCount(cmdparts[n]);

	return code;
}

static Tcl_Obj* TclValueAliasCreate(TclValueType *vtype, Tcl_Obj *intRep) {	
	/* create an alias for the object command in the master */

	char aliasName[50 + TCL_INTEGER_SPACE];
	static int valuecntr = 0; /* not thread safe */

	sprintf(aliasName, "::tclvalue::ValueCmd%d", ++valuecntr);

	/* Create alias */
	int code = Tcl_CreateAliasObj(vtype -> masterInterp, aliasName, vtype -> slaveInterp, Tcl_GetString(intRep), 0, NULL); 
	
	if (code != TCL_OK) {
		/* raise exception, might leak memory */
		return NULL;
	}

	return Tcl_NewStringObj(aliasName, -1); 
}

/* call out into the Tcl scripts associated with this type 
 * stubs for now */
static void		DupTclValueInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr) {
	fprintf(stderr, "Duplicate %p -> %p\n", srcPtr, copyPtr);
	/* for now just copy the intrep. Need support from the type */
	TclValueType *vtype = (TclValueType*) srcPtr -> typePtr;
	
	Tcl_Obj *intRep = SlaveObjCommand(srcPtr);
	
	/* duplicate object. First construct empty, then call clone */
	int code = invoke(vtype -> slaveInterp, vtype -> typeCmd, vtype -> constructor, NULL);
	if (code != TCL_OK) {
		/* phew.... */
		fprintf(stderr, "Error in Standard constructor :( %s", Tcl_GetStringResult(vtype -> slaveInterp));
		/* crash... */
		copyPtr -> typePtr = NULL;
		return;
	}
	
	Tcl_Obj *newIntRep = Tcl_GetObjResult(vtype -> slaveInterp);
	Tcl_IncrRefCount(newIntRep);

	/* call the clone method on the new value */
	code = invoke(vtype -> slaveInterp, newIntRep, vtype -> clonemethod, intRep);
	if (code != TCL_OK) {
		/* phew.... */
		fprintf(stderr, "Error in clone method :( %s", Tcl_GetStringResult(vtype -> slaveInterp));
	}
	
	Tcl_Obj *aliasCmd = TclValueAliasCreate(vtype, newIntRep);
	Tcl_IncrRefCount(aliasCmd);

	SlaveObjCommand(copyPtr) = newIntRep;
	MasterObjCommand(copyPtr) = aliasCmd;
	
	copyPtr -> typePtr = (Tcl_ObjType*) vtype;
}


static void		FreeTclValueInternalRep(Tcl_Obj *valuePtr) {
    /* call destructor of the object */
	fprintf(stderr, "Free %p \n", valuePtr);
	TclValueType *vtype = (TclValueType*)valuePtr -> typePtr;
	Tcl_Obj *intRep = SlaveObjCommand(valuePtr);
	Tcl_Obj *aliasCmd = MasterObjCommand(valuePtr);
	
	/* call the constructor with the current obj as a parameter */
	int code = invoke(vtype->slaveInterp, intRep, vtype->destructor, NULL);
	if (code != TCL_OK) {
		/* phew - may not fail */
		fprintf(stderr, "Ouch: %s\n", Tcl_GetStringResult(vtype->slaveInterp));
		return;
	}
	
	/* delete the alias
	 * urks: Don't have interp, maybe it is unsafe to mess with it here :/ */
	Tcl_Command cmd=Tcl_GetCommandFromObj(vtype->masterInterp, aliasCmd);
	Tcl_DeleteCommandFromToken(vtype->masterInterp, cmd);
	


	/* free the memory */
	Tcl_DecrRefCount(intRep);
	Tcl_DecrRefCount(aliasCmd);
}

static void		UpdateStringOfTclValue(Tcl_Obj *valuePtr) {
	/* call the constructor with the current obj as a parameter */
	fprintf(stderr, "Update string %p\n", valuePtr);
	TclValueType *vtype = (TclValueType*) valuePtr -> typePtr;
	Tcl_Obj *instanceCmd = SlaveObjCommand(valuePtr);
	
	int code = invoke(vtype->slaveInterp, instanceCmd, vtype -> reprmethod, NULL);
	
	if (code != TCL_OK) {
		/* it *may* not fail. Instead of panic, leave the error message 
		 * as a replacement value in there for debugging */
	}

	/* the result is the string rep. Too bad we cannot have
	 * a list rep, nor make the output string proc write 
	 * the string rep into our Tcl_Obj */
	Tcl_Obj *result = Tcl_GetObjResult(vtype -> slaveInterp);
	const char *resultStr = Tcl_GetStringFromObj(result, &valuePtr -> length);

	valuePtr -> bytes = ckalloc(valuePtr->length+1);
	memcpy(valuePtr -> bytes, resultStr, valuePtr->length);
	valuePtr -> bytes[valuePtr->length]=0; /* terminate, just in case... */
}

/* Register a new type */
static int RegisterObjTypeCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "type definition");
		return TCL_ERROR;
	}
	
	Tcl_Interp *slaveInterp = clientData;
	Tcl_Obj *typenameobj = objv[1];
	const char *typename = Tcl_GetString(typenameobj);
	/* check if this objtype already exists, if yes, refuse */

	Tcl_ObjType *type=Tcl_GetObjType(typename);
	if (type != NULL) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj("type already exists", -1));
		return TCL_ERROR;
	} else {
		/* evaluate the definition as an oo::class in the slave */
		Tcl_Obj *cmdobj[4];
		cmdobj[0]=Tcl_NewStringObj("oo::class", -1);
		cmdobj[1]=Tcl_NewStringObj("create", -1);
		cmdobj[2]=typenameobj;
		cmdobj[3]=objv[2];

		for (int n=0; n<4; n++) Tcl_IncrRefCount(cmdobj[n]);
		
		int code = Tcl_EvalObjv(slaveInterp, 4, cmdobj, TCL_EVAL_GLOBAL);
		
		for (int n=0; n<4; n++) Tcl_DecrRefCount(cmdobj[n]);
		
		if (code!=TCL_OK) { return TCL_ERROR; }

		/* we attach a cargo here */
		TclValueType *type = (TclValueType *) ckalloc(sizeof(TclValueType));
		type->objType = TclValueTclType_template;
		type->objType.name = strdup(typename);
		Tcl_RegisterObjType(&type->objType);

		type -> slaveInterp = slaveInterp;
		type -> masterInterp = interp;

		type->typeCmd = objv[1];
		Tcl_IncrRefCount(type->typeCmd);
		
		type->constructor = Tcl_NewStringObj("new", -1);
		Tcl_IncrRefCount(type->constructor);

		type->destructor = Tcl_NewStringObj("destroy", -1);
		Tcl_IncrRefCount(type->constructor);

		type->clonemethod = Tcl_NewStringObj("clone", -1);
		Tcl_IncrRefCount(type->clonemethod);

		type->reprmethod = Tcl_NewStringObj("repr", -1);
		Tcl_IncrRefCount(type->reprmethod);

	}

	return TCL_OK;	
}

static int ShimmerCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "value type");
		return TCL_ERROR;
	}
	Tcl_Obj *value = objv[1];
	Tcl_Obj *typeObj = objv[2];

	fprintf(stderr, "Shimmer %p\n", value);
	/* first attempt to convert the 2nd argument to value type */
	if (SetTcl_ObjTypeFromAny(interp, typeObj) != TCL_OK) {
		return TCL_ERROR;
	}
	
	/* Do not allow shimmering to a non-scripted type */
	if (! typeObj -> internalRep.ptrAndLongRep.value) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj("Not a scripted type", -1));
		return TCL_ERROR;
	}
	
	TclValueType *vtype = typeObj -> internalRep.ptrAndLongRep.ptr;
	
	/* If the object is already the target type, just return the intRep */
	if (value -> typePtr == (Tcl_ObjType *)vtype) {
		Tcl_SetObjResult(interp, MasterObjCommand(value));
		return TCL_OK;
	}

	/* call the constructor with the current obj as a parameter */
	int code = invoke(vtype -> slaveInterp, vtype -> typeCmd, vtype -> constructor, value);
	
	if (code != TCL_OK) {
		Tcl_SetObjResult(interp, Tcl_GetObjResult(vtype -> slaveInterp));
		return TCL_ERROR;
	}

	/* if everything went smooth, we now have a token for the intrep 
	 * which should be a object instance command */
	Tcl_Obj *intRep = Tcl_GetObjResult(vtype -> slaveInterp);
	Tcl_IncrRefCount(intRep);
	
	/* create an alias for the object command in the master */
	Tcl_Obj *aliasCmd = TclValueAliasCreate(vtype, intRep);
	if (aliasCmd == NULL) {
		return TCL_ERROR;
	}

	/* Remove the old intrep */
	TclFreeIntRep(value);

	value -> typePtr = (Tcl_ObjType *)vtype;
	SlaveObjCommand(value) = intRep;
	MasterObjCommand(value) = aliasCmd;
	Tcl_IncrRefCount(aliasCmd);

	Tcl_SetObjResult(interp, aliasCmd);
	return TCL_OK;
}

/* Returns intRep (master obj command) if this is a scripted type. 
 * Doesn't shimmer */
static int GetIntRepCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "value");
		return TCL_ERROR;
	}
	Tcl_Obj *value = objv[1];

	fprintf(stderr, "GetIntRep %p\n", value);
	

	/* If the object is already the target type, just return the intRep */
	Tcl_ObjType *vtype = value -> typePtr;

	if (vtype->freeIntRepProc == FreeTclValueInternalRep) {
		/* It is one of our types */
		Tcl_SetObjResult(interp, MasterObjCommand(value));
		return TCL_OK;
	} else {
		/* return an empty object */
		Tcl_SetObjResult(interp, Tcl_NewObj());
		return TCL_OK;
	}
}

/* Returns intRep (slave obj command) if this is a scripted type. 
 * Doesn't shimmer */
static int GetSlaveIntRepCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "value");
		return TCL_ERROR;
	}
	Tcl_Obj *value = objv[1];

	fprintf(stderr, "GetSlaveIntRep %p\n", value);
	

	/* If the object is already the target type, just return the intRep */
	Tcl_ObjType *vtype = value -> typePtr;

	if (vtype->freeIntRepProc == FreeTclValueInternalRep) {
		/* It is one of our types */
		Tcl_SetObjResult(interp, SlaveObjCommand(value));
		return TCL_OK;
	} else {
		/* return an empty object */
		Tcl_SetObjResult(interp, Tcl_NewObj());
		return TCL_OK;
	}
}

static int InvalidateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "value");
		return TCL_ERROR;
	}

	Tcl_InvalidateStringRep(objv[1]);
	return TCL_OK;
}

/* unshare returns a copy of a value, if it is shared.
 * Otherwise, return the value itself */
static int UnshareCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "variable");
		return TCL_ERROR;
	}
	
	Tcl_Obj *varName = objv[1];
	Tcl_Obj *value = Tcl_ObjGetVar2(interp, varName, NULL, TCL_LEAVE_ERR_MSG);
	if (value == NULL) {
		return TCL_ERROR;
	}

	/* unshare object, if shared */
	if (Tcl_IsShared(value)) {
		value = Tcl_DuplicateObj(value);
	}

	Tcl_SetObjResult(interp, value);
	return TCL_OK;
}

/* new returns a new value of a specific type that is not shared.
 * It calls the type's constructor with the excess arguments */
static int NewCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "typename ?args ...?");
		return TCL_ERROR;
	}
	
	Tcl_Obj *typeObj = objv[1];

	/* first attempt to convert the 2nd argument to value type */
	if (SetTcl_ObjTypeFromAny(interp, typeObj) != TCL_OK) {
		return TCL_ERROR;
	}
	
	/* Do not allow shimmering to a non-scripted type */
	if (! typeObj -> internalRep.ptrAndLongRep.value) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj("Not a scripted type", -1));
		return TCL_ERROR;
	}
	
	TclValueType *vtype = typeObj -> internalRep.ptrAndLongRep.ptr;
	
	Tcl_Obj* result=Tcl_NewObj();

	fprintf(stderr, "New %p\n", result);
	
	/* call the constructor with the excess args */
	
	int nargs = objc;
	Tcl_Obj **cmdparts = (Tcl_Obj*)Tcl_Alloc(sizeof(Tcl_Obj*)*nargs);

	cmdparts[0]=vtype->typeCmd;
	cmdparts[1]=vtype->constructor;

	Tcl_IncrRefCount(cmdparts[0]);
	Tcl_IncrRefCount(cmdparts[1]);

	for (int n=2; n<nargs; n++) {
		cmdparts[n]=objv[n];
		Tcl_IncrRefCount(cmdparts[n]);
	}

	int code = Tcl_EvalObjv(vtype->slaveInterp, nargs, cmdparts, TCL_EVAL_GLOBAL);

	for (int n=0; n<objc; n++) { 
		Tcl_DecrRefCount(cmdparts[n]);
	}

	if (code != TCL_OK) {
		Tcl_SetObjResult(interp, Tcl_GetObjResult(vtype -> slaveInterp));
		return TCL_ERROR;
	}

	/* if it was successful, now set the fields in the newborn object  */
	/* if everything went smooth, we now have a token for the intrep 
	 * which should be a object instance command */
	Tcl_Obj *intRep = Tcl_GetObjResult(vtype -> slaveInterp);
	Tcl_IncrRefCount(intRep);
	
	/* create an alias for the object command in the master */
	Tcl_Obj *aliasCmd = TclValueAliasCreate(vtype, intRep);
	if (aliasCmd == NULL) {
		return TCL_ERROR;
	}

	/* Remove the old intrep */
/*	TclFreeIntRep(value); a new object doesn't have an intrep */

	Tcl_InvalidateStringRep(result);
	result -> typePtr = (Tcl_ObjType *)vtype;
	SlaveObjCommand(result) = intRep;
	MasterObjCommand(result) = aliasCmd;
	Tcl_IncrRefCount(aliasCmd);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;

}


int Tclvalue_Init(Tcl_Interp* interp) {
	if (interp == 0) return TCL_ERROR;

	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}

	Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);

	Tcl_RegisterObjType(&Tcl_ObjTypeType);

	/* create namespace for commands */
	if (Tcl_Eval(interp, "namespace eval tclvalue {}")!=TCL_OK) {
		return TCL_ERROR;
	}
	
	Tcl_Interp *slaveInterp = Tcl_CreateSlave(interp, "tclvalue::interp", 0);
	Tcl_CreateObjCommand(interp, "tclvalue::register", RegisterObjTypeCmd, slaveInterp , NULL);
	Tcl_CreateObjCommand(interp, "tclvalue::shimmer", ShimmerCmd, slaveInterp, NULL);
	Tcl_CreateObjCommand(interp, "tclvalue::getIntRep", GetIntRepCmd, slaveInterp, NULL); 
	Tcl_CreateObjCommand(interp, "tclvalue::getSlaveIntRep", GetSlaveIntRepCmd, slaveInterp, NULL); 
	Tcl_CreateObjCommand(interp, "tclvalue::invalidate", InvalidateCmd, slaveInterp, NULL);
	Tcl_CreateObjCommand(interp, "tclvalue::unshare", UnshareCmd, slaveInterp, NULL);
	Tcl_CreateObjCommand(interp, "tclvalue::new", NewCmd, slaveInterp, NULL);
	
#ifdef LIST_INJECT
	/* copy list object proc from list type */
	Tcl_ObjType* tclListType =  (Tcl_ObjType *) Tcl_GetObjType("list");	
	listSetFromAny = tclListType -> setFromAnyProc;

	/* inject list conversion code 
	 * WARNING may break Tcl 
	 */
	tclListType->setFromAnyProc = SetListFromNumArray;
	Tcl_RegisterObjType(tclListType); 
#endif

	return TCL_OK;
}

