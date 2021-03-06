5 stdcall MsiAdvertiseProductA(str str str long)
6 stdcall MsiAdvertiseProductW(wstr wstr wstr long)
7 stdcall MsiCloseAllHandles()
8 stdcall MsiCloseHandle(long)
9 stdcall MsiCollectUserInfoA(str)
10 stdcall MsiCollectUserInfoW(wstr)
11 stdcall MsiConfigureFeatureA(str str long)
12 stub MsiConfigureFeatureFromDescriptorA
13 stub MsiConfigureFeatureFromDescriptorW
14 stdcall MsiConfigureFeatureW(wstr wstr ptr)
15 stdcall MsiConfigureProductA(str long long)
16 stdcall MsiConfigureProductW(wstr long long)
17 stdcall MsiCreateRecord(long)
18 stdcall MsiDatabaseApplyTransformA(long str long)
19 stdcall MsiDatabaseApplyTransformW(long wstr long)
20 stdcall MsiDatabaseCommit(long)
21 stdcall MsiDatabaseExportA(long str str str)
22 stdcall MsiDatabaseExportW(long wstr wstr wstr)
23 stdcall MsiDatabaseGenerateTransformA(long long str long long)
24 stdcall MsiDatabaseGenerateTransformW(long long wstr long long)
25 stdcall MsiDatabaseGetPrimaryKeysA(long str ptr)
26 stdcall MsiDatabaseGetPrimaryKeysW(long wstr ptr)
27 stdcall MsiDatabaseImportA(str str long)
28 stdcall MsiDatabaseImportW(wstr wstr long)
29 stub MsiDatabaseMergeA
30 stub MsiDatabaseMergeW
31 stdcall MsiDatabaseOpenViewA(long str ptr)
32 stdcall MsiDatabaseOpenViewW(long wstr ptr)
33 stdcall MsiDoActionA(long str)
34 stdcall MsiDoActionW(long wstr)
35 stdcall MsiEnableUIPreview(long ptr)
36 stdcall MsiEnumClientsA(str long ptr)
37 stdcall MsiEnumClientsW(wstr long ptr)
38 stdcall MsiEnumComponentQualifiersA(str long ptr ptr ptr ptr)
39 stdcall MsiEnumComponentQualifiersW(wstr long ptr ptr ptr ptr)
40 stdcall MsiEnumComponentsA(long ptr)
41 stdcall MsiEnumComponentsW(long ptr)
42 stdcall MsiEnumFeaturesA(str long ptr ptr)
43 stdcall MsiEnumFeaturesW(wstr long ptr ptr)
44 stdcall MsiEnumProductsA(long ptr)
45 stdcall MsiEnumProductsW(long ptr)
46 stdcall MsiEvaluateConditionA(long str)
47 stdcall MsiEvaluateConditionW(long wstr)
48 stdcall MsiGetLastErrorRecord()
49 stdcall MsiGetActiveDatabase(long)
50 stdcall MsiGetComponentStateA(long str ptr ptr)
51 stdcall MsiGetComponentStateW(long wstr ptr ptr)
52 stdcall MsiGetDatabaseState(long)
53 stub MsiGetFeatureCostA
54 stub MsiGetFeatureCostW
55 stub MsiGetFeatureInfoA
56 stub MsiGetFeatureInfoW
57 stdcall MsiGetFeatureStateA(long str ptr ptr)
58 stdcall MsiGetFeatureStateW(long wstr ptr ptr)
59 stdcall MsiGetFeatureUsageA(str str ptr ptr)
60 stdcall MsiGetFeatureUsageW(wstr wstr ptr ptr)
61 stub MsiGetFeatureValidStatesA
62 stub MsiGetFeatureValidStatesW
63 stdcall MsiGetLanguage(long)
64 stdcall MsiGetMode(long long)
65 stdcall MsiGetProductCodeA(str str)
66 stdcall MsiGetProductCodeW(wstr wstr)
67 stdcall MsiGetProductInfoA(str str str long)
68 stub MsiGetProductInfoFromScriptA
69 stub MsiGetProductInfoFromScriptW
70 stdcall MsiGetProductInfoW(wstr wstr wstr long)
71 stdcall MsiGetProductPropertyA(long str ptr ptr)
72 stdcall MsiGetProductPropertyW(long wstr ptr ptr)
73 stdcall MsiGetPropertyA(ptr str ptr ptr)
74 stdcall MsiGetPropertyW(ptr wstr ptr ptr)
75 stdcall MsiGetSourcePathA(long str ptr ptr)
76 stdcall MsiGetSourcePathW(long wstr ptr ptr)
77 stdcall MsiGetSummaryInformationA(long str long ptr)
78 stdcall MsiGetSummaryInformationW(long wstr long ptr)
79 stdcall MsiGetTargetPathA(long str ptr ptr)
80 stdcall MsiGetTargetPathW(long wstr ptr ptr)
81 stdcall MsiGetUserInfoA(str ptr ptr ptr ptr ptr ptr)
82 stdcall MsiGetUserInfoW(wstr ptr ptr ptr ptr ptr ptr)
83 stub MsiInstallMissingComponentA
84 stub MsiInstallMissingComponentW
85 stub MsiInstallMissingFileA
86 stub MsiInstallMissingFileW
87 stdcall MsiInstallProductA(str str)
88 stdcall MsiInstallProductW(wstr wstr)
89 stdcall MsiLocateComponentA(str ptr long)
90 stdcall MsiLocateComponentW(wstr ptr long)
91 stdcall MsiOpenDatabaseA(str str ptr)
92 stdcall MsiOpenDatabaseW(wstr wstr ptr)
93 stdcall MsiOpenPackageA(str ptr)
94 stdcall MsiOpenPackageW(wstr ptr)
95 stdcall MsiOpenProductA(str ptr)
96 stdcall MsiOpenProductW(wstr ptr)
97 stdcall MsiPreviewBillboardA(long str str)
98 stdcall MsiPreviewBillboardW(long wstr wstr)
99 stdcall MsiPreviewDialogA(long str)
100 stdcall MsiPreviewDialogW(long wstr)
101 stub MsiProcessAdvertiseScriptA
102 stub MsiProcessAdvertiseScriptW
103 stdcall MsiProcessMessage(long long long)
104 stub MsiProvideComponentA
105 stdcall MsiProvideComponentFromDescriptorA(str ptr ptr ptr)
106 stdcall MsiProvideComponentFromDescriptorW(wstr ptr ptr ptr)
107 stub MsiProvideComponentW
108 stdcall MsiProvideQualifiedComponentA(str str long ptr ptr)
109 stdcall MsiProvideQualifiedComponentW(wstr wstr long ptr ptr)
110 stdcall MsiQueryFeatureStateA(str str)
111 stdcall MsiQueryFeatureStateW(wstr wstr)
112 stdcall MsiQueryProductStateA(str)
113 stdcall MsiQueryProductStateW(wstr)
114 stdcall MsiRecordDataSize(long long)
115 stdcall MsiRecordGetFieldCount(long)
116 stdcall MsiRecordGetInteger(long long)
117 stdcall MsiRecordGetStringA(long long ptr ptr)
118 stdcall MsiRecordGetStringW(long long ptr ptr)
119 stdcall MsiRecordIsNull(long long)
120 stdcall MsiRecordReadStream(long long ptr ptr)
121 stdcall MsiRecordSetInteger(long long long)
122 stdcall MsiRecordSetStreamA(long long str)
123 stdcall MsiRecordSetStreamW(long long wstr)
124 stdcall MsiRecordSetStringA(long long str)
125 stdcall MsiRecordSetStringW(long long wstr)
126 stdcall MsiReinstallFeatureA(str str long)
127 stub MsiReinstallFeatureFromDescriptorA
128 stub MsiReinstallFeatureFromDescriptorW
129 stdcall MsiReinstallFeatureW(wstr wstr long)
130 stdcall MsiReinstallProductA(str long)
131 stdcall MsiReinstallProductW(wstr long)
132 stdcall MsiSequenceA(long str long)
133 stdcall MsiSequenceW(long wstr long)
134 stdcall MsiSetComponentStateA(long str long)
135 stdcall MsiSetComponentStateW(long wstr long)
136 stdcall MsiSetExternalUIA(ptr long ptr)
137 stdcall MsiSetExternalUIW(ptr long ptr)
138 stdcall MsiSetFeatureStateA(long str long)
139 stdcall MsiSetFeatureStateW(long wstr long)
140 stdcall MsiSetInstallLevel(long long)
141 stdcall MsiSetInternalUI(long ptr)
142 stub MsiVerifyDiskSpace
143 stdcall MsiSetMode(long long long)
144 stdcall MsiSetPropertyA(long str str)
145 stdcall MsiSetPropertyW(long wstr wstr)
146 stdcall MsiSetTargetPathA(long str str)
147 stdcall MsiSetTargetPathW(long wstr wstr)
148 stdcall MsiSummaryInfoGetPropertyA(long long ptr ptr ptr ptr ptr)
149 stdcall MsiSummaryInfoGetPropertyCount(long ptr)
150 stdcall MsiSummaryInfoGetPropertyW(long long ptr ptr ptr ptr ptr)
151 stdcall MsiSummaryInfoPersist(long)
152 stdcall MsiSummaryInfoSetPropertyA(long long long long ptr str)
153 stdcall MsiSummaryInfoSetPropertyW(long long long long ptr wstr)
154 stdcall MsiUseFeatureA(str str)
155 stdcall MsiUseFeatureW(wstr wstr)
156 stdcall MsiVerifyPackageA(str)
157 stdcall MsiVerifyPackageW(wstr)
158 stdcall MsiViewClose(long)
159 stdcall MsiViewExecute(long long)
160 stdcall MsiViewFetch(long ptr)
161 stdcall MsiViewGetErrorA(long ptr ptr)
162 stdcall MsiViewGetErrorW(long ptr ptr)
163 stdcall MsiViewModify(long long long)
164 stdcall MsiDatabaseIsTablePersistentA(long str)
165 stdcall MsiDatabaseIsTablePersistentW(long wstr)
166 stdcall MsiViewGetColumnInfo(long long ptr)
167 stdcall MsiRecordClearData(long)
168 stdcall MsiEnableLogA(long str long)
169 stdcall MsiEnableLogW(long wstr long)
170 stdcall MsiFormatRecordA(long long ptr ptr)
171 stdcall MsiFormatRecordW(long long ptr ptr)
172 stdcall MsiGetComponentPathA(str str ptr ptr)
173 stdcall MsiGetComponentPathW(wstr wstr ptr ptr)
174 stdcall MsiApplyPatchA(str str long str)
175 stdcall MsiApplyPatchW(wstr wstr long wstr)
176 stdcall MsiAdvertiseScriptA(str long ptr long)
177 stdcall MsiAdvertiseScriptW(wstr long ptr long)
178 stub MsiGetPatchInfoA
179 stub MsiGetPatchInfoW
180 stdcall MsiEnumPatchesA(str long ptr ptr ptr)
181 stdcall MsiEnumPatchesW(str long ptr ptr ptr)
182 stdcall -private DllGetVersion(ptr)
183 stub MsiGetProductCodeFromPackageCodeA
184 stub MsiGetProductCodeFromPackageCodeW
185 stub MsiCreateTransformSummaryInfoA
186 stub MsiCreateTransformSummaryInfoW
187 stub MsiQueryFeatureStateFromDescriptorA
188 stub MsiQueryFeatureStateFromDescriptorW
189 stdcall MsiConfigureProductExA(str long long str)
190 stdcall MsiConfigureProductExW(wstr long long wstr)
191 stub MsiInvalidateFeatureCache
192 stdcall MsiUseFeatureExA(str str long long)
193 stdcall MsiUseFeatureExW(wstr wstr long long)
194 stdcall MsiGetFileVersionA(str str ptr str ptr)
195 stdcall MsiGetFileVersionW(wstr wstr ptr wstr ptr)
196 stdcall MsiLoadStringA(long long long long long)
197 stdcall MsiLoadStringW(long long long long long)
198 stdcall MsiMessageBoxA(long long long long long long)
199 stdcall MsiMessageBoxW(long long long long long long)
200 stdcall MsiDecomposeDescriptorA(str ptr ptr ptr ptr)
201 stdcall MsiDecomposeDescriptorW(wstr ptr ptr ptr ptr)
202 stdcall MsiProvideQualifiedComponentExA(str str long str long long ptr ptr)
203 stdcall MsiProvideQualifiedComponentExW(wstr wstr long wstr long long ptr ptr)
204 stdcall MsiEnumRelatedProductsA(str long long ptr)
205 stdcall MsiEnumRelatedProductsW(wstr long long ptr)
206 stub MsiSetFeatureAttributesA
207 stub MsiSetFeatureAttributesW
208 stdcall MsiSourceListClearAllA(str str long)
209 stdcall MsiSourceListClearAllW(wstr wstr long)
210 stdcall MsiSourceListAddSourceA(str str long str)
211 stdcall MsiSourceListAddSourceW(wstr wstr long wstr)
212 stub MsiSourceListForceResolutionA
213 stub MsiSourceListForceResolutionW
214 stub MsiIsProductElevatedA
215 stub MsiIsProductElevatedW
216 stdcall MsiGetShortcutTargetA(str ptr ptr ptr)
217 stdcall MsiGetShortcutTargetW(wstr ptr ptr ptr)
218 stdcall MsiGetFileHashA(str long ptr)
219 stdcall MsiGetFileHashW(wstr long ptr)
220 stub MsiEnumComponentCostsA
221 stub MsiEnumComponentCostsW
222 stdcall MsiCreateAndVerifyInstallerDirectory(long)
223 stdcall MsiGetFileSignatureInformationA(str long ptr ptr ptr)
224 stdcall MsiGetFileSignatureInformationW(wstr long ptr ptr ptr)
225 stdcall MsiProvideAssemblyA(str str long long str ptr)
226 stdcall MsiProvideAssemblyW(wstr wstr long long wstr ptr)
227 stdcall MsiAdvertiseProductExA(str str str long long long)
228 stdcall MsiAdvertiseProductExW(wstr wstr wstr long long long)
229 stub MsiNotifySidChangeA
230 stub MsiNotifySidChangeW
231 stdcall MsiOpenPackageExA(str long ptr)
232 stdcall MsiOpenPackageExW(wstr long ptr)
233 stub MsiDeleteUserDataA
234 stub MsiDeleteUserDataW
235 stub Migrate10CachedPackagesA
236 stub Migrate10CachedPackagesW

@ stdcall -private DllCanUnloadNow()
@ stdcall -private DllGetClassObject(ptr ptr ptr)
@ stdcall -private DllRegisterServer()
@ stdcall -private DllUnregisterServer()
