/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SKY_ENGINE_CORE_CSS_CSSIMAGEVALUE_H_
#define SKY_ENGINE_CORE_CSS_CSSIMAGEVALUE_H_

#include "sky/engine/core/css/CSSValue.h"
#include "sky/engine/core/fetch/ResourceFetcher.h"
#include "sky/engine/platform/weborigin/Referrer.h"
#include "sky/engine/wtf/RefPtr.h"

namespace blink {

class Document;
class Element;
class KURL;
class StyleFetchedImage;
class StyleImage;
class RenderObject;

class CSSImageValue : public CSSValue {
public:
    static PassRefPtr<CSSImageValue> create(const KURL& url, StyleImage* image = 0)
    {
        return adoptRef(new CSSImageValue(url, url, image));
    }
    static PassRefPtr<CSSImageValue> create(const String& rawValue, const KURL& url, StyleImage* image = 0)
    {
        return adoptRef(new CSSImageValue(rawValue, url, image));
    }
    ~CSSImageValue();

    StyleFetchedImage* cachedImage(ResourceFetcher*, const ResourceLoaderOptions&);
    StyleFetchedImage* cachedImage(ResourceFetcher* fetcher) { return cachedImage(fetcher, ResourceFetcher::defaultResourceOptions()); }
    // Returns a StyleFetchedImage if the image is cached already, otherwise a StylePendingImage.
    StyleImage* cachedOrPendingImage();

    const String& url() { return m_absoluteURL; }

    void setReferrer(const Referrer& referrer) { m_referrer = referrer; }
    const Referrer& referrer() const { return m_referrer; }

    void reResolveURL(const Document&);

    String customCSSText() const;

    PassRefPtr<CSSValue> cloneForCSSOM() const;

    bool equals(const CSSImageValue&) const;

    bool knownToBeOpaque(const RenderObject*) const;

    void setInitiator(const AtomicString& name) { m_initiatorName = name; }
    void restoreCachedResourceIfNeeded(Document&);

private:
    CSSImageValue(const String& rawValue, const KURL&, StyleImage*);

    String m_relativeURL;
    String m_absoluteURL;
    Referrer m_referrer;
    RefPtr<StyleImage> m_image;
    bool m_accessedImage;
    AtomicString m_initiatorName;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSImageValue, isImageValue());

} // namespace blink

#endif  // SKY_ENGINE_CORE_CSS_CSSIMAGEVALUE_H_
