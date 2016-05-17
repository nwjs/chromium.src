/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "public/platform/WebSecurityOrigin.h"

#include "platform/weborigin/DatabaseIdentifier.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebSecurityOriginPrivate : public SecurityOrigin {
};

WebSecurityOrigin WebSecurityOrigin::createFromDatabaseIdentifier(const WebString& databaseIdentifier)
{
    return WebSecurityOrigin(createSecurityOriginFromDatabaseIdentifier(databaseIdentifier));
}

WebSecurityOrigin WebSecurityOrigin::createFromString(const WebString& origin)
{
    return WebSecurityOrigin(SecurityOrigin::createFromString(origin));
}

WebSecurityOrigin WebSecurityOrigin::create(const WebURL& url)
{
    return WebSecurityOrigin(SecurityOrigin::create(url));
}

WebSecurityOrigin WebSecurityOrigin::createFromTuple(const WebString& protocol, const WebString& host, int port)
{
    return WebSecurityOrigin(SecurityOrigin::create(protocol, host, port));
}

WebSecurityOrigin WebSecurityOrigin::createUnique()
{
    return WebSecurityOrigin(SecurityOrigin::createUnique());
}

void WebSecurityOrigin::reset()
{
    assign(0);
}

void WebSecurityOrigin::assign(const WebSecurityOrigin& other)
{
    WebSecurityOriginPrivate* p = const_cast<WebSecurityOriginPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

WebString WebSecurityOrigin::protocol() const
{
    ASSERT(m_private);
    return m_private->protocol();
}

WebString WebSecurityOrigin::host() const
{
    ASSERT(m_private);
    return m_private->host();
}

unsigned short WebSecurityOrigin::port() const
{
    ASSERT(m_private);
    return m_private->port();
}

unsigned short WebSecurityOrigin::effectivePort() const
{
    ASSERT(m_private);
    return m_private->effectivePort();
}

bool WebSecurityOrigin::isUnique() const
{
    ASSERT(m_private);
    return m_private->isUnique();
}

bool WebSecurityOrigin::canAccess(const WebSecurityOrigin& other) const
{
    ASSERT(m_private);
    ASSERT(other.m_private);
    return m_private->canAccess(other.m_private);
}

bool WebSecurityOrigin::canRequest(const WebURL& url) const
{
    ASSERT(m_private);
    return m_private->canRequest(url);
}

bool WebSecurityOrigin::isPotentiallyTrustworthy(WebString& errorMessage) const
{
    ASSERT(m_private);
    bool result = m_private->isPotentiallyTrustworthy();
    if (!result)
        errorMessage = WTF::String(m_private->isPotentiallyTrustworthyErrorMessage());
    return result;
}

WebString WebSecurityOrigin::toString() const
{
    ASSERT(m_private);
    return m_private->toString();
}

WebString WebSecurityOrigin::databaseIdentifier() const
{
    ASSERT(m_private);
    return createDatabaseIdentifierFromSecurityOrigin(m_private);
}

bool WebSecurityOrigin::canAccessPasswordManager() const
{
    ASSERT(m_private);
    return m_private->canAccessPasswordManager();
}

WebSecurityOrigin::WebSecurityOrigin(const WTF::PassRefPtr<SecurityOrigin>& origin)
    : m_private(static_cast<WebSecurityOriginPrivate*>(origin.leakRef()))
{
}

WebSecurityOrigin& WebSecurityOrigin::operator=(const WTF::PassRefPtr<SecurityOrigin>& origin)
{
    assign(static_cast<WebSecurityOriginPrivate*>(origin.leakRef()));
    return *this;
}

WebSecurityOrigin::operator WTF::PassRefPtr<SecurityOrigin>() const
{
    return PassRefPtr<SecurityOrigin>(const_cast<WebSecurityOriginPrivate*>(m_private));
}

SecurityOrigin* WebSecurityOrigin::get() const
{
    return m_private;
}

void WebSecurityOrigin::assign(WebSecurityOriginPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

void WebSecurityOrigin::grantLoadLocalResources() const
{
    get()->grantLoadLocalResources();
}

void WebSecurityOrigin::grantUniversalAccess() const
{
    get()->grantUniversalAccess();
}

} // namespace blink
