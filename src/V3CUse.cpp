// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Handle SV classes
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
// V3CUse's Transformations:
//
// Each module:
//   Each cell:
//      Create CUse for cell forward declaration
//   Search for dtypes referencing class, and create CUse for forward declaration
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3CUse.h"

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################

// Visit within a module all nodes and data types they reference, finding
// any classes so we can make sure they are defined when Verilated code
// compiles
class CUseVisitor final : public VNVisitorConst {
    // NODE STATE
    //  AstNode::user1()     -> bool.  True if already visited
    const VNUser1InUse m_inuser1;

    // MEMBERS
    AstNodeModule* const m_modp;  // Current module
    std::map<std::string, std::pair<FileLine*, VUseType>> m_didUse;  // What we already used
    std::unordered_map<const AstCFunc*, const AstNodeModule*> m_owner;  // ownerOf() memo

    // METHODS
    void addNewUse(const AstNode* nodep, VUseType useType, const string& name) {
        auto e = m_didUse.emplace(name, std::make_pair(nodep->fileline(), useType));
        if (e.second || ((e.first->second.second & useType) != useType)) {
            e.first->second.second = e.first->second.second | useType;
        }
    }

    // VISITORS
    const AstNodeModule* ownerOf(const AstCFunc* funcp) {
        // The module a function is defined in. backp() is the previous sibling or, at a
        // list's head, the parent, so following it ends at the module; memoized, as a
        // module's functions are long sibling lists.
        const auto it = m_owner.find(funcp);
        if (it != m_owner.end()) return it->second;
        const AstNode* nodep = funcp;
        while (nodep && !VN_IS(nodep, NodeModule)) nodep = nodep->backp();
        return m_owner[funcp] = VN_CAST(nodep, NodeModule);
    }
    void addClassUse(const AstNode* nodep, const AstClass* classp) {
        // A class's declaration: forward for headers; its own header for the module's
        // implementation, which no longer gets every class through __Syms.h
        addNewUse(nodep, VUseType::INT_FWD_CLASS, classp->name());
        addNewUse(nodep, VUseType::IMP_INCLUDE, classp->name());
    }
    void visit(AstClassRefDType* nodep) override { addClassUse(nodep, nodep->classp()); }
    void visit(AstCFunc* nodep) override {
        if (nodep->user1SetOnce()) return;
        iterateAndNextConstNull(nodep->argsp());
        iterateAndNextConstNull(nodep->stmtsp());
    }
    void visit(AstNodeCCall* nodep) override {
        // A call into a class's method needs that class's header, and the classes of the
        // callee's parameters: an argument it is passed (VlNull{} for a handle) is converted
        // to the parameter's type here. The callee's body is its own module's business.
        if (nodep->user1SetOnce()) return;
        if (const AstCFunc* const funcp = nodep->funcp()) {
            if (const AstClass* const classp = VN_CAST(ownerOf(funcp), Class))
                addClassUse(nodep, classp);
            for (const AstNode* argp = funcp->argsp(); argp; argp = argp->nextp()) {
                if (argp->dtypep()) iterateConst(argp->dtypep());
            }
        }
        if (nodep->dtypep()) iterateConst(nodep->dtypep());
        iterateChildrenConst(nodep);
    }
    void visit(AstCReturn* nodep) override {
        UASSERT_OBJ(!nodep->user1SetOnce(), nodep, "Visited same return twice");
        iterateConst(nodep->lhsp()->dtypep());
    }
    void visit(AstNodeDType* nodep) override {
        if (nodep->user1SetOnce()) return;  // Process once
        if (nodep->virtRefDTypep()) iterateConst(nodep->virtRefDTypep());
        if (nodep->virtRefDType2p()) iterateConst(nodep->virtRefDType2p());
        // Through a typedef, and into an element type, to find the classes a container
        // holds (VlAssocArray<..., VlClassRef<C>> needs C declared where it's used)
        if (AstNodeDType* const skippedp = nodep->skipRefp()) {
            if (skippedp != nodep) iterateConst(skippedp);
        }
        if (AstNodeDType* const subp = nodep->subDTypep()) iterateConst(subp);

        // Add a CUse for every struct that requires a declaration
        AstNodeUOrStructDType* const stypep = VN_CAST(nodep->skipRefp(), NodeUOrStructDType);
        if (stypep && stypep->classOrPackagep()) {
            addNewUse(nodep, VUseType::INT_INCLUDE, stypep->classOrPackagep()->name());
            iterateChildrenConst(stypep);
        } else if (const AstClassRefDType* const classp
                   = VN_CAST(nodep->skipRefp(), ClassRefDType)) {
            addClassUse(nodep, classp->classp());
        }
    }
    void visit(AstNode* nodep) override {
        if (nodep->user1SetOnce()) return;  // Process once
        if (nodep->dtypep()) iterateConst(nodep->dtypep());
        iterateChildrenConst(nodep);
    }
    void visit(AstCell* nodep) override {
        if (nodep->user1SetOnce()) return;  // Process once
        // Currently no IMP_INCLUDE because we include __Syms which has them all
        addNewUse(nodep, VUseType::INT_FWD_CLASS, nodep->modp()->name());
        iterateChildrenConst(nodep);
    }

public:
    // CONSTRUCTORS
    explicit CUseVisitor(AstNodeModule* modp)
        : m_modp{modp} {
        iterateConst(modp);
        if (AstClass* const classp = VN_CAST(modp, Class)) {
            // clone() resets the class's embedded covergroups, inherited ones included
            // (EmitCUtil::cloneDefinition), so it needs their classes too
            classp->foreachMember([&](AstClass*, AstVar* varp) {
                const AstClassRefDType* const refp
                    = VN_CAST(varp->dtypep()->skipRefp(), ClassRefDType);
                if (refp && refp->classp()->isCovergroup()) addClassUse(varp, refp->classp());
            });
        }

        for (const auto& used : m_didUse) {
            AstCUse* const newp = new AstCUse{used.second.first, used.second.second, used.first};
            m_modp->addStmtsp(newp);
            UINFO(8, "Insert " << newp);
        }
    }
    ~CUseVisitor() override = default;
    VL_UNCOPYABLE(CUseVisitor);
};

//######################################################################
// Class class functions

void V3CUse::cUseAll() {
    UINFO(2, __FUNCTION__ << ":");
    // Call visitor separately for each module, so visitor state is cleared
    for (AstNodeModule* modp = v3Global.rootp()->modulesp(); modp;
         modp = VN_AS(modp->nextp(), NodeModule)) {
        // Insert under this module; someday we should e.g. make Ast
        // for each output file and put under that
        CUseVisitor{modp};
    }
    V3Global::dumpCheckGlobalTree("cuse", 0, dumpTreeEitherLevel() >= 3);
}
