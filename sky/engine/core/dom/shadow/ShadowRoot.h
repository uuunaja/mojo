/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SKY_ENGINE_CORE_DOM_SHADOW_SHADOWROOT_H_
#define SKY_ENGINE_CORE_DOM_SHADOW_SHADOWROOT_H_

#include "sky/engine/core/dom/ContainerNode.h"
#include "sky/engine/core/dom/DocumentFragment.h"
#include "sky/engine/core/dom/Element.h"
#include "sky/engine/core/dom/TreeScope.h"

namespace blink {

class Document;
class ElementShadow;
class ExceptionState;
class InsertionPoint;

class ShadowRoot final : public DocumentFragment, public TreeScope {
    DEFINE_WRAPPERTYPEINFO();
public:

    static PassRefPtr<ShadowRoot> create(Document& document)
    {
        return adoptRef(new ShadowRoot(document));
    }

    void recalcStyle(StyleRecalcChange);

    // Disambiguate between Node and TreeScope hierarchies; TreeScope's implementation is simpler.
    using TreeScope::document;
    using TreeScope::getElementById;

    Element* host() const { return toElement(parentOrShadowHostNode()); }
    ElementShadow* owner() const { return host() ? host()->shadow() : 0; }

    virtual void insertedInto(ContainerNode*) override;
    virtual void removedFrom(ContainerNode*) override;

    bool containsContentElements() const;
    bool containsShadowRoots() const;

    void didAddInsertionPoint();
    void didRemoveInsertionPoint();
    const Vector<RefPtr<InsertionPoint> >& descendantInsertionPoints();

    // Make protected methods from base class public here.
    using TreeScope::setDocument;
    using TreeScope::setParentTreeScope;

    Element* activeElement() const;

    PassRefPtr<Node> cloneNode(bool, ExceptionState&);
    PassRefPtr<Node> cloneNode(ExceptionState& exceptionState);

private:
    explicit ShadowRoot(Document&);
    virtual ~ShadowRoot();

    virtual void dispose() override;

    void addChildShadowRoot();
    void removeChildShadowRoot();
    void invalidateDescendantInsertionPoints();

    virtual PassRefPtr<Node> cloneNode(bool) override;

    Vector<RefPtr<InsertionPoint> > m_descendantInsertionPoints;
    unsigned m_descendantContentElementCount;
    unsigned m_childShadowRootCount;
    unsigned m_descendantInsertionPointsIsValid : 1;
};

inline Element* ShadowRoot::activeElement() const
{
    return adjustedFocusedElement();
}

DEFINE_NODE_TYPE_CASTS(ShadowRoot, isShadowRoot());
DEFINE_TYPE_CASTS(ShadowRoot, TreeScope, treeScope, treeScope->rootNode().isShadowRoot(), treeScope.rootNode().isShadowRoot());

} // namespace blink

#endif  // SKY_ENGINE_CORE_DOM_SHADOW_SHADOWROOT_H_
