#ifndef BOOST_CERTIFY_DETAIL_CONFIG_HPP
#define BOOST_CERTIFY_DETAIL_CONFIG_HPP

#ifndef BOOST_CERTIFY_USE_NATIVE_CERTIFICATE_STORE
#define BOOST_CERTIFY_USE_NATIVE_CERTIFICATE_STORE 1
#endif // BOOST_CERTIFY_USE_NATIVE_CERTIFICATE_STORE

#ifndef BOOST_CERTIFY_SEPARATE_COMPILATION
#define BOOST_CERTIFY_HEADER_ONLY 1
#ifndef BOOST_CERTIFY_DECL
#define BOOST_CERTIFY_DECL inline
#endif // BOOST_CERTIFY_DECL
#else
#define BOOST_CERTIFY_HEADER_ONLY 0
#ifndef BOOST_CERTIFY_DECL
#define BOOST_CERTIFY_DECL
#endif // BOOST_CERTIFY_DECL
#endif // BOOST_CERTIFY_SEPARATE_COMPILATION

#endif // BOOST_CERTIFY_DETAIL_CONFIG_HPP
