/*
 * Copyright 2006 Juan Lang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#ifndef __WINE_CRYPTRES_H__
#define __WINE_CRYPTRES_H__

#define IDS_AUTHORITY_KEY_ID 1000
#define IDS_KEY_ATTRIBUTES 1001
#define IDS_KEY_USAGE_RESTRICTION 1002
#define IDS_SUBJECT_ALT_NAME 1003
#define IDS_ISSUER_ALT_NAME 1004
#define IDS_BASIC_CONSTRAINTS 1005
#define IDS_KEY_USAGE 1006
#define IDS_CERT_POLICIES 1007
#define IDS_SUBJECT_KEY_IDENTIFIER 1008
#define IDS_CRL_REASON_CODE 1009
#define IDS_CRL_DIST_POINTS 1010
#define IDS_ENHANCED_KEY_USAGE 1011
#define IDS_AUTHORITY_INFO_ACCESS 1012
#define IDS_CERT_EXTENSIONS 1013
#define IDS_NEXT_UPDATE_LOCATION 1014
#define IDS_YES_OR_NO_TRUST 1015
#define IDS_EMAIL_ADDRESS 1016
#define IDS_UNSTRUCTURED_NAME 1017
#define IDS_CONTENT_TYPE 1018
#define IDS_MESSAGE_DIGEST 1019
#define IDS_SIGNING_TIME 1020
#define IDS_COUNTER_SIGN 1021
#define IDS_CHALLENGE_PASSWORD 1022
#define IDS_UNSTRUCTURED_ADDRESS 1023
#define IDS_SMIME_CAPABILITIES 1024
#define IDS_PREFER_SIGNED_DATA 1025
#define IDS_CPS 1026
#define IDS_USER_NOTICE 1027
#define IDS_OCSP 1028
#define IDS_CA_ISSUER 1029
#define IDS_CERT_TEMPLATE_NAME 1030
#define IDS_CERT_TYPE 1031
#define IDS_CERT_MANIFOLD 1032
#define IDS_NETSCAPE_CERT_TYPE 1033
#define IDS_NETSCAPE_BASE_URL 1034
#define IDS_NETSCAPE_REVOCATION_URL 1035
#define IDS_NETSCAPE_CA_REVOCATION_URL 1036
#define IDS_NETSCAPE_CERT_RENEWAL_URL 1037
#define IDS_NETSCAPE_CA_POLICY_URL 1038
#define IDS_NETSCAPE_SSL_SERVER_NAME 1039
#define IDS_NETSCAPE_COMMENT 1040
#define IDS_SPC_SP_AGENCY_INFO 1041
#define IDS_SPC_FINANCIAL_CRITERIA 1042
#define IDS_SPC_MINIMAL_CRITERIA 1043
#define IDS_COUNTRY 1044
#define IDS_ORGANIZATION 1045
#define IDS_ORGANIZATIONAL_UNIT 1046
#define IDS_COMMON_NAME 1047
#define IDS_LOCALITY 1048
#define IDS_STATE_OR_PROVINCE 1049
#define IDS_TITLE 1050
#define IDS_GIVEN_NAME 1051
#define IDS_INITIALS 1052
#define IDS_SUR_NAME 1053
#define IDS_DOMAIN_COMPONENT 1054
#define IDS_STREET_ADDRESS 1055
#define IDS_SERIAL_NUMBER 1056
#define IDS_CA_VERSION 1057
#define IDS_CROSS_CA_VERSION 1058
#define IDS_SERIALIZED_SIG_SERIAL_NUMBER 1059
#define IDS_PRINCIPAL_NAME 1060
#define IDS_WINDOWS_PRODUCT_UPDATE 1061
#define IDS_ENROLLMENT_NAME_VALUE_PAIR 1062
#define IDS_OS_VERSION 1063
#define IDS_ENROLLMENT_CSP 1064
#define IDS_CRL_NUMBER 1065
#define IDS_DELTA_CRL_INDICATOR 1066
#define IDS_ISSUING_DIST_POINT 1067
#define IDS_FRESHEST_CRL 1068
#define IDS_NAME_CONSTRAINTS 1069
#define IDS_POLICY_MAPPINGS 1070
#define IDS_POLICY_CONSTRAINTS 1071
#define IDS_CROSS_CERT_DIST_POINTS 1072
#define IDS_APPLICATION_POLICIES 1073
#define IDS_APPLICATION_POLICY_MAPPINGS 1074
#define IDS_APPLICATION_POLICY_CONSTRAINTS 1075
#define IDS_CMC_DATA 1076
#define IDS_CMC_RESPONSE 1077
#define IDS_UNSIGNED_CMC_REQUEST 1078
#define IDS_CMC_STATUS_INFO 1079
#define IDS_CMC_EXTENSIONS 1080
#define IDS_CMC_ATTRIBUTES 1081
#define IDS_PKCS_7_DATA 1082
#define IDS_PKCS_7_SIGNED 1083
#define IDS_PKCS_7_ENVELOPED 1084
#define IDS_PKCS_7_SIGNED_ENVELOPED 1085
#define IDS_PKCS_7_DIGESTED 1086
#define IDS_PKCS_7_ENCRYPTED 1087
#define IDS_PREVIOUS_CA_CERT_HASH 1088
#define IDS_CRL_VIRTUAL_BASE 1089
#define IDS_CRL_NEXT_PUBLISH 1090
#define IDS_CA_EXCHANGE 1091
#define IDS_KEY_RECOVERY_AGENT 1092
#define IDS_CERTIFICATE_TEMPLATE 1093
#define IDS_ENTERPRISE_ROOT_OID 1094
#define IDS_RDN_DUMMY_SIGNER 1095
#define IDS_ARCHIVED_KEY_ATTR 1096
#define IDS_CRL_SELF_CDP 1097
#define IDS_REQUIRE_CERT_CHAIN_POLICY 1098
#define IDS_TRANSACTION_ID 1099
#define IDS_SENDER_NONCE 1100
#define IDS_RECIPIENT_NONCE 1101
#define IDS_REG_INFO 1102
#define IDS_GET_CERTIFICATE 1103
#define IDS_GET_CRL 1104
#define IDS_REVOKE_REQUEST 1105
#define IDS_QUERY_PENDING 1106
#define IDS_SORTED_CTL 1107
#define IDS_ARCHIVED_KEY_CERT_HASH 1108
#define IDS_PRIVATE_KEY_USAGE_PERIOD 1109
#define IDS_CLIENT_INFORMATION 1110
#define IDS_SERVER_AUTHENTICATION 1111
#define IDS_CLIENT_AUTHENTICATION 1112
#define IDS_CODE_SIGNING 1113
#define IDS_SECURE_EMAIL 1114
#define IDS_TIME_STAMPING 1115
#define IDS_MICROSOFT_TRUST_LIST_SIGNING 1116
#define IDS_MICROSOFT_TIME_STAMPING 1117
#define IDS_IPSEC_END_SYSTEM 1118
#define IDS_IPSEC_TUNNEL 1119
#define IDS_IPSEC_USER 1120
#define IDS_EFS 1121
#define IDS_WHQL_CRYPTO 1122
#define IDS_NT5_CRYPTO 1123
#define IDS_OEM_WHQL_CRYPTO 1124
#define IDS_EMBEDDED_NT_CRYPTO 1125
#define IDS_KEY_PACK_LICENSES 1126
#define IDS_LICENSE_SERVER 1127
#define IDS_SMART_CARD_LOGON 1128
#define IDS_DIGITAL_RIGHTS 1129
#define IDS_QUALIFIED_SUBORDINATION 1130
#define IDS_KEY_RECOVERY 1131
#define IDS_DOCUMENT_SIGNING 1132
#define IDS_IPSEC_IKE_INTERMEDIATE 1133
#define IDS_FILE_RECOVERY 1134
#define IDS_ROOT_LIST_SIGNER 1135
#define IDS_ANY_APPLICATION_POLICIES 1136
#define IDS_DS_EMAIL_REPLICATION 1137
#define IDS_ENROLLMENT_AGENT 1138
#define IDS_LIFETIME_SIGNING 1139
#define IDS_ANY_CERT_POLICY 1140

#endif /* ndef __WINE_CRYPTRES_H__ */