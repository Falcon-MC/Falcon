#pragma once

struct ssl_ctx_st;

namespace TlsRoots {

    /**
     * Trusts the root certificates of the operating system in the context: the default OpenSSL paths, the Windows
     * root store and the Android system store. The certificates are read once and shared by every context.
     */
    void load(ssl_ctx_st *context);

}
