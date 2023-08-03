;
; Note: shader requires additional functionality:
;       Use native low precision
;
; shader debug name: d261a0b234bcd862c8f2920effb8e419.pdb
; shader hash: d261a0b234bcd862c8f2920effb8e419
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%0 = type { void (i32)*, i32 (i32, i32, i8**, i32, i32)*, void (i32, i32*, i32)*, void (i32, i32, i32, i32, i32, i32, i32)*, void (i32, i32, i8*, i32)*, void (i8*, i32, i32*, i32)*, i32 (i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)*, void (i32, i8*, i32)*, void (i32, i8*)*, i32 (i32, i32)*, i32 (i32, i32, i32)*, i32 (i32, i32, i32, i32)*, float (i32, float)*, float (i32, float, float)*, float (i32, float, float, float)*, i1 (i32, float)*, void (i32, i32, i32, i32, i8*)*, void (i8*, i32, i32, i32, i32)*, i8* (i32, i32)* }
%1 = type { i32, i32, i32, i32 }
%___rpsl_node_info_struct = type <{ i32, i32, i32, i32, i32 }>
%___rpsl_entry_desc_struct = type <{ i32, i32, i32, i32, i8*, i8* }>
%___rpsl_type_info_struct = type <{ i8, i8, i8, i8, i32, i32, i32 }>
%___rpsl_params_info_struct = type <{ i32, i32, i32, i32, i32, i16, i16 }>
%___rpsl_shader_ref_struct = type <{ i32, i32, i32, i32 }>
%___rpsl_pipeline_info_struct = type <{ i32, i32, i32, i32 }>
%___rpsl_pipeline_field_info_struct = type <{ i32, i32, i32, i32, i32, i32, i32, i32 }>
%___rpsl_pipeline_res_binding_info_struct = type <{ i32, i32, i32, i32 }>
%___rpsl_module_info_struct = type <{ i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, [137 x i8]*, [4 x %___rpsl_node_info_struct]*, [4 x %___rpsl_type_info_struct]*, [10 x %___rpsl_params_info_struct]*, [3 x %___rpsl_entry_desc_struct]*, [1 x %___rpsl_shader_ref_struct]*, [1 x %___rpsl_pipeline_info_struct]*, [1 x %___rpsl_pipeline_field_info_struct]*, [1 x %___rpsl_pipeline_res_binding_info_struct]*, i32 }>
%struct.RpsParameterDesc = type { %struct.RpsTypeInfo, i32, %1*, i8*, i32 }
%struct.RpsTypeInfo = type { i16, i16 }
%struct.RpsNodeDesc = type { i32, i32, %struct.RpsParameterDesc*, i8* }
%struct.RpslEntry = type { i8*, void (i32, i8**, i32)*, %struct.RpsParameterDesc*, %struct.RpsNodeDesc*, i32, i32 }
%struct.texture = type { i32, i32, i32, i32, %struct.SubresourceRange, float, i32 }
%struct.SubresourceRange = type { i16, i16, i32, i32 }
%struct.ResourceDesc = type { i32, i32, i32, i32, i32, i32, i32, i32, i32 }

@s_rpslRuntimeProcs = global %0 zeroinitializer
@___rpsl_nodedefs_hello_triangle = private constant [4 x %___rpsl_node_info_struct] [%___rpsl_node_info_struct <{ i32 0, i32 68, i32 0, i32 2, i32 1 }>, %___rpsl_node_info_struct <{ i32 1, i32 80, i32 2, i32 1, i32 1 }>, %___rpsl_node_info_struct <{ i32 2, i32 89, i32 3, i32 3, i32 1 }>, %___rpsl_node_info_struct zeroinitializer], align 4
@___rpsl_entries_hello_triangle = private constant [3 x %___rpsl_entry_desc_struct] [%___rpsl_entry_desc_struct <{ i32 0, i32 107, i32 6, i32 1, i8* bitcast (void (%struct.texture*)* @rpsl_M_hello_triangle_Fn_main to i8*), i8* bitcast (void (i32, i8**, i32)* @rpsl_M_hello_triangle_Fn_main_wrapper to i8*) }>, %___rpsl_entry_desc_struct <{ i32 1, i32 123, i32 7, i32 2, i8* bitcast (void (%struct.texture*, float)* @rpsl_M_hello_triangle_Fn_mainBreathing to i8*), i8* bitcast (void (i32, i8**, i32)* @rpsl_M_hello_triangle_Fn_mainBreathing_wrapper to i8*) }>, %___rpsl_entry_desc_struct zeroinitializer], align 4
@___rpsl_types_metadata_hello_triangle = private constant [4 x %___rpsl_type_info_struct] [%___rpsl_type_info_struct <{ i8 6, i8 0, i8 0, i8 0, i32 0, i32 36, i32 4 }>, %___rpsl_type_info_struct <{ i8 4, i8 32, i8 0, i8 4, i32 0, i32 16, i32 4 }>, %___rpsl_type_info_struct <{ i8 4, i8 32, i8 0, i8 0, i32 0, i32 4, i32 4 }>, %___rpsl_type_info_struct zeroinitializer], align 4
@___rpsl_params_metadata_hello_triangle = private constant [10 x %___rpsl_params_info_struct] [%___rpsl_params_info_struct <{ i32 15, i32 0, i32 272629888, i32 -1, i32 0, i16 36, i16 0 }>, %___rpsl_params_info_struct <{ i32 17, i32 1, i32 0, i32 -1, i32 0, i16 16, i16 36 }>, %___rpsl_params_info_struct <{ i32 22, i32 0, i32 128, i32 -1, i32 0, i16 36, i16 0 }>, %___rpsl_params_info_struct <{ i32 22, i32 0, i32 128, i32 -1, i32 0, i16 36, i16 0 }>, %___rpsl_params_info_struct <{ i32 35, i32 2, i32 0, i32 -1, i32 0, i16 4, i16 36 }>, %___rpsl_params_info_struct <{ i32 54, i32 2, i32 0, i32 -1, i32 0, i16 4, i16 40 }>, %___rpsl_params_info_struct <{ i32 112, i32 0, i32 524288, i32 -1, i32 0, i16 36, i16 0 }>, %___rpsl_params_info_struct <{ i32 112, i32 0, i32 524288, i32 -1, i32 0, i16 36, i16 0 }>, %___rpsl_params_info_struct <{ i32 54, i32 2, i32 0, i32 -1, i32 0, i16 4, i16 36 }>, %___rpsl_params_info_struct zeroinitializer], align 4
@___rpsl_shader_refs_hello_triangle = private constant [1 x %___rpsl_shader_ref_struct] zeroinitializer, align 4
@___rpsl_pipelines_hello_triangle = private constant [1 x %___rpsl_pipeline_info_struct] zeroinitializer, align 4
@___rpsl_pipeline_fields_hello_triangle = private constant [1 x %___rpsl_pipeline_field_info_struct] zeroinitializer, align 4
@___rpsl_pipeline_res_bindings_hello_triangle = private constant [1 x %___rpsl_pipeline_res_binding_info_struct] zeroinitializer, align 4
@___rpsl_string_table_hello_triangle = constant [137 x i8] c"hello_triangle\00t\00data\00renderTarget\00oneOverAspectRatio\00timeInSeconds\00clear_color\00Triangle\00TriangleBreathing\00main\00backbuffer\00mainBreathing\00", align 4
@___rpsl_module_info_hello_triangle = dllexport constant %___rpsl_module_info_struct <{ i32 1297305682, i32 3, i32 9, i32 0, i32 137, i32 3, i32 3, i32 9, i32 2, i32 0, i32 0, i32 0, i32 0, [137 x i8]* @___rpsl_string_table_hello_triangle, [4 x %___rpsl_node_info_struct]* @___rpsl_nodedefs_hello_triangle, [4 x %___rpsl_type_info_struct]* @___rpsl_types_metadata_hello_triangle, [10 x %___rpsl_params_info_struct]* @___rpsl_params_metadata_hello_triangle, [3 x %___rpsl_entry_desc_struct]* @___rpsl_entries_hello_triangle, [1 x %___rpsl_shader_ref_struct]* @___rpsl_shader_refs_hello_triangle, [1 x %___rpsl_pipeline_info_struct]* @___rpsl_pipelines_hello_triangle, [1 x %___rpsl_pipeline_field_info_struct]* @___rpsl_pipeline_fields_hello_triangle, [1 x %___rpsl_pipeline_res_binding_info_struct]* @___rpsl_pipeline_res_bindings_hello_triangle, i32 1297305682 }>, align 4
@"@@rps_Str0" = private unnamed_addr constant [12 x i8] c"clear_color\00"
@"@@rps_Str1" = private unnamed_addr constant [2 x i8] c"t\00"
@"@@rps_ParamAttr2" = private constant %1 { i32 272629888, i32 0, i32 0, i32 0 }, align 4
@"@@rps_Str3" = private unnamed_addr constant [5 x i8] c"data\00"
@"@@rps_ParamAttr4" = private constant %1 { i32 0, i32 0, i32 27, i32 0 }, align 4
@"@@rps_ParamDescArray5" = private constant [2 x %struct.RpsParameterDesc] [%struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 36, i16 64 }, i32 0, %1* @"@@rps_ParamAttr2", i8* getelementptr inbounds ([2 x i8], [2 x i8]* @"@@rps_Str1", i32 0, i32 0), i32 4 }, %struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 16, i16 0 }, i32 0, %1* @"@@rps_ParamAttr4", i8* getelementptr inbounds ([5 x i8], [5 x i8]* @"@@rps_Str3", i32 0, i32 0), i32 0 }], align 4
@"@@rps_Str6" = private unnamed_addr constant [9 x i8] c"Triangle\00"
@"@@rps_ParamAttr8" = private constant %1 { i32 128, i32 0, i32 35, i32 0 }, align 4
@"@@rps_ParamDescArray9" = private constant [1 x %struct.RpsParameterDesc] [%struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 36, i16 64 }, i32 0, %1* @"@@rps_ParamAttr8", i8* getelementptr inbounds ([13 x i8], [13 x i8]* @"@@rps_Str11", i32 0, i32 0), i32 4 }], align 4
@"@@rps_Str10" = private unnamed_addr constant [18 x i8] c"TriangleBreathing\00"
@"@@rps_Str11" = private unnamed_addr constant [13 x i8] c"renderTarget\00"
@"@@rps_ParamAttr12" = private constant %1 { i32 128, i32 0, i32 35, i32 0 }, align 4
@"@@rps_Str13" = private unnamed_addr constant [19 x i8] c"oneOverAspectRatio\00"
@"@@rps_ParamAttr14" = private constant %1 zeroinitializer, align 4
@"@@rps_ParamAttr16" = private constant %1 zeroinitializer, align 4
@"@@rps_ParamDescArray17" = private constant [3 x %struct.RpsParameterDesc] [%struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 36, i16 64 }, i32 0, %1* @"@@rps_ParamAttr12", i8* getelementptr inbounds ([13 x i8], [13 x i8]* @"@@rps_Str11", i32 0, i32 0), i32 4 }, %struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 4, i16 0 }, i32 0, %1* @"@@rps_ParamAttr14", i8* getelementptr inbounds ([19 x i8], [19 x i8]* @"@@rps_Str13", i32 0, i32 0), i32 0 }, %struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 4, i16 0 }, i32 0, %1* @"@@rps_ParamAttr16", i8* getelementptr inbounds ([14 x i8], [14 x i8]* @"@@rps_Str25", i32 0, i32 0), i32 0 }], align 4
@NodeDecls_hello_triangle = dllexport constant [3 x %struct.RpsNodeDesc] [%struct.RpsNodeDesc { i32 1, i32 2, %struct.RpsParameterDesc* getelementptr inbounds ([2 x %struct.RpsParameterDesc], [2 x %struct.RpsParameterDesc]* @"@@rps_ParamDescArray5", i32 0, i32 0), i8* getelementptr inbounds ([12 x i8], [12 x i8]* @"@@rps_Str0", i32 0, i32 0) }, %struct.RpsNodeDesc { i32 1, i32 1, %struct.RpsParameterDesc* getelementptr inbounds ([1 x %struct.RpsParameterDesc], [1 x %struct.RpsParameterDesc]* @"@@rps_ParamDescArray9", i32 0, i32 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @"@@rps_Str6", i32 0, i32 0) }, %struct.RpsNodeDesc { i32 1, i32 3, %struct.RpsParameterDesc* getelementptr inbounds ([3 x %struct.RpsParameterDesc], [3 x %struct.RpsParameterDesc]* @"@@rps_ParamDescArray17", i32 0, i32 0), i8* getelementptr inbounds ([18 x i8], [18 x i8]* @"@@rps_Str10", i32 0, i32 0) }], align 4
@"@@rps_Str18" = private unnamed_addr constant [5 x i8] c"main\00"
@"@@rps_ParamAttr20" = private constant %1 { i32 524288, i32 0, i32 0, i32 0 }, align 4
@"@@rps_ParamDescArray21" = private constant [1 x %struct.RpsParameterDesc] [%struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 36, i16 64 }, i32 0, %1* @"@@rps_ParamAttr20", i8* getelementptr inbounds ([11 x i8], [11 x i8]* @"@@rps_Str23", i32 0, i32 0), i32 4 }], align 4
@"rpsl_M_hello_triangle_E_main@value" = constant %struct.RpslEntry { i8* getelementptr inbounds ([5 x i8], [5 x i8]* @"@@rps_Str18", i32 0, i32 0), void (i32, i8**, i32)* @rpsl_M_hello_triangle_Fn_main_wrapper, %struct.RpsParameterDesc* getelementptr inbounds ([1 x %struct.RpsParameterDesc], [1 x %struct.RpsParameterDesc]* @"@@rps_ParamDescArray21", i32 0, i32 0), %struct.RpsNodeDesc* getelementptr inbounds ([3 x %struct.RpsNodeDesc], [3 x %struct.RpsNodeDesc]* @NodeDecls_hello_triangle, i32 0, i32 0), i32 1, i32 3 }, align 4
@rpsl_M_hello_triangle_E_main = dllexport constant %struct.RpslEntry* @"rpsl_M_hello_triangle_E_main@value", align 4
@rpsl_M_hello_triangle_E_main_pp = dllexport constant %struct.RpslEntry** @rpsl_M_hello_triangle_E_main, align 4
@"@@rps_Str22" = private unnamed_addr constant [14 x i8] c"mainBreathing\00"
@"@@rps_Str23" = private unnamed_addr constant [11 x i8] c"backbuffer\00"
@"@@rps_ParamAttr24" = private constant %1 { i32 524288, i32 0, i32 0, i32 0 }, align 4
@"@@rps_Str25" = private unnamed_addr constant [14 x i8] c"timeInSeconds\00"
@"@@rps_ParamAttr26" = private constant %1 zeroinitializer, align 4
@"@@rps_ParamDescArray27" = private constant [2 x %struct.RpsParameterDesc] [%struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 36, i16 64 }, i32 0, %1* @"@@rps_ParamAttr24", i8* getelementptr inbounds ([11 x i8], [11 x i8]* @"@@rps_Str23", i32 0, i32 0), i32 4 }, %struct.RpsParameterDesc { %struct.RpsTypeInfo { i16 4, i16 0 }, i32 0, %1* @"@@rps_ParamAttr26", i8* getelementptr inbounds ([14 x i8], [14 x i8]* @"@@rps_Str25", i32 0, i32 0), i32 0 }], align 4
@"rpsl_M_hello_triangle_E_mainBreathing@value" = constant %struct.RpslEntry { i8* getelementptr inbounds ([14 x i8], [14 x i8]* @"@@rps_Str22", i32 0, i32 0), void (i32, i8**, i32)* @rpsl_M_hello_triangle_Fn_mainBreathing_wrapper, %struct.RpsParameterDesc* getelementptr inbounds ([2 x %struct.RpsParameterDesc], [2 x %struct.RpsParameterDesc]* @"@@rps_ParamDescArray27", i32 0, i32 0), %struct.RpsNodeDesc* getelementptr inbounds ([3 x %struct.RpsNodeDesc], [3 x %struct.RpsNodeDesc]* @NodeDecls_hello_triangle, i32 0, i32 0), i32 2, i32 3 }, align 4
@rpsl_M_hello_triangle_E_mainBreathing = dllexport constant %struct.RpslEntry* @"rpsl_M_hello_triangle_E_mainBreathing@value", align 4
@rpsl_M_hello_triangle_E_mainBreathing_pp = dllexport constant %struct.RpslEntry** @rpsl_M_hello_triangle_E_mainBreathing, align 4
@rpsl_M_entry_tbl = dllexport constant [3 x i8*] [i8* getelementptr inbounds ([5 x i8], [5 x i8]* @"@@rps_Str18", i32 0, i32 0), i8* getelementptr inbounds ([14 x i8], [14 x i8]* @"@@rps_Str22", i32 0, i32 0), i8* null], align 4
@rpsl_M_entry_tbl_ptr = dllexport constant [3 x i8*]* @rpsl_M_entry_tbl, align 4
@"@@rps_Str28" = private unnamed_addr constant [15 x i8] c"hello_triangle\00"
@rpsl_M_module_id = dllexport constant i8* getelementptr inbounds ([15 x i8], [15 x i8]* @"@@rps_Str28", i32 0, i32 0), align 4
@rpsl_M_module_id_ptr = dllexport constant i8** @rpsl_M_module_id, align 4

; Function Attrs: nounwind
define internal fastcc void @"\01?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z"(%struct.texture* noalias nocapture sret %agg.result, i32 %resourceHdl, %struct.ResourceDesc* nocapture readonly %desc) #0 {
entry:
  call void @llvm.dbg.declare(metadata %struct.ResourceDesc* %desc, metadata !156, metadata !157), !dbg !158 ; var:"desc" !DIExpression() func:"make_default_texture_view_from_desc"
  call void @llvm.dbg.value(metadata i32 %resourceHdl, i64 0, metadata !159, metadata !157), !dbg !160 ; var:"resourceHdl" !DIExpression() func:"make_default_texture_view_from_desc"
  %MipLevels = getelementptr inbounds %struct.ResourceDesc, %struct.ResourceDesc* %desc, i32 0, i32 6, !dbg !161 ; line:158 col:14
  %0 = load i32, i32* %MipLevels, align 4, !dbg !161, !tbaa !162 ; line:158 col:14
  %Type = getelementptr inbounds %struct.ResourceDesc, %struct.ResourceDesc* %desc, i32 0, i32 0, !dbg !166 ; line:157 col:15
  %1 = load i32, i32* %Type, align 4, !dbg !166, !tbaa !162 ; line:157 col:15
  %cmp = icmp eq i32 %1, 4, !dbg !167 ; line:157 col:20
  br i1 %cmp, label %cond.end, label %cond.false, !dbg !168 ; line:157 col:9

cond.false:                                       ; preds = %entry
  %DepthOrArraySize = getelementptr inbounds %struct.ResourceDesc, %struct.ResourceDesc* %desc, i32 0, i32 5, !dbg !169 ; line:157 col:54
  %2 = load i32, i32* %DepthOrArraySize, align 4, !dbg !169, !tbaa !162 ; line:157 col:54
  br label %cond.end, !dbg !168 ; line:157 col:9

cond.end:                                         ; preds = %cond.false, %entry
  %cond = phi i32 [ %2, %cond.false ], [ 1, %entry ], !dbg !168 ; line:157 col:9
  call void @llvm.dbg.value(metadata i32 %0, i64 0, metadata !170, metadata !157), !dbg !171 ; var:"numMips" !DIExpression() func:"make_default_texture_view"
  call void @llvm.dbg.value(metadata i32 %cond, i64 0, metadata !173, metadata !157), !dbg !174 ; var:"arraySlices" !DIExpression() func:"make_default_texture_view"
  call void @llvm.dbg.value(metadata i32 %resourceHdl, i64 0, metadata !175, metadata !157), !dbg !176 ; var:"resourceHdl" !DIExpression() func:"make_default_texture_view"
  %conv.i = trunc i32 %0 to i16, !dbg !177 ; line:128 col:56
  %3 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 0, !dbg !178 ; line:134 col:5
  store i32 %resourceHdl, i32* %3, align 4, !dbg !178 ; line:134 col:5
  %4 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 1, !dbg !178 ; line:134 col:5
  store i32 0, i32* %4, align 4, !dbg !178 ; line:134 col:5
  %5 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 2, !dbg !178 ; line:134 col:5
  store i32 0, i32* %5, align 4, !dbg !178 ; line:134 col:5
  %6 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 3, !dbg !178 ; line:134 col:5
  store i32 0, i32* %6, align 4, !dbg !178 ; line:134 col:5
  %7 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 4, i32 0, !dbg !178 ; line:134 col:5
  store i16 0, i16* %7, align 4, !dbg !178 ; line:134 col:5
  %8 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 4, i32 1, !dbg !178 ; line:134 col:5
  store i16 %conv.i, i16* %8, align 2, !dbg !178 ; line:134 col:5
  %9 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 4, i32 2, !dbg !178 ; line:134 col:5
  store i32 0, i32* %9, align 4, !dbg !178 ; line:134 col:5
  %10 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 4, i32 3, !dbg !178 ; line:134 col:5
  store i32 %cond, i32* %10, align 4, !dbg !178 ; line:134 col:5
  %11 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 5, !dbg !178 ; line:134 col:5
  store float 0.000000e+00, float* %11, align 4, !dbg !178 ; line:134 col:5
  %12 = getelementptr inbounds %struct.texture, %struct.texture* %agg.result, i32 0, i32 6, !dbg !178 ; line:134 col:5
  store i32 50462976, i32* %12, align 4, !dbg !178 ; line:134 col:5
  ret void, !dbg !179 ; line:154 col:5
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
define void @rpsl_M_hello_triangle_Fn_main(%struct.texture* %backbuffer) #0 {
entry:
  call void @___rpsl_block_marker(i32 0, i32 0, i32 0, i32 2, i32 -1, i32 0, i32 -1) #0
  call void @llvm.dbg.declare(metadata %struct.texture* %backbuffer, metadata !180, metadata !157), !dbg !181 ; var:"backbuffer" !DIExpression() func:"main"
  call void @llvm.dbg.value(metadata <4 x float> <float 0.000000e+00, float 0x3FC99999A0000000, float 0x3FD99999A0000000, float 1.000000e+00>, i64 0, metadata !182, metadata !157), !dbg !183 ; var:"val" !DIExpression() func:"clear"
  call void @llvm.dbg.declare(metadata %struct.texture* %backbuffer, metadata !185, metadata !157) #0, !dbg !186 ; var:"t" !DIExpression() func:"clear"
  %0 = alloca [2 x i8*], align 4, !dbg !187 ; line:294 col:12
  %.sub = getelementptr inbounds [2 x i8*], [2 x i8*]* %0, i32 0, i32 0
  %1 = bitcast i8** %.sub to %struct.texture**, !dbg !187 ; line:294 col:12
  store %struct.texture* %backbuffer, %struct.texture** %1, align 4, !dbg !187 ; line:294 col:12
  %2 = alloca <4 x float>, align 4, !dbg !187 ; line:294 col:12
  store <4 x float> <float 0.000000e+00, float 0x3FC99999A0000000, float 0x3FD99999A0000000, float 1.000000e+00>, <4 x float>* %2, align 4, !dbg !187 ; line:294 col:12
  %3 = getelementptr [2 x i8*], [2 x i8*]* %0, i32 0, i32 1, !dbg !187 ; line:294 col:12
  %4 = bitcast i8** %3 to <4 x float>**, !dbg !187 ; line:294 col:12
  store <4 x float>* %2, <4 x float>** %4, align 4, !dbg !187 ; line:294 col:12
  %5 = call i32 @___rpsl_node_call(i32 0, i32 2, i8** %.sub, i32 0, i32 0) #0, !dbg !187 ; line:294 col:12
  %6 = alloca i8*, align 4, !dbg !188 ; line:15 col:5
  %7 = bitcast i8** %6 to %struct.texture**, !dbg !188 ; line:15 col:5
  store %struct.texture* %backbuffer, %struct.texture** %7, align 4, !dbg !188 ; line:15 col:5
  %8 = call i32 @___rpsl_node_call(i32 1, i32 1, i8** nonnull %6, i32 0, i32 1) #0, !dbg !188 ; line:15 col:5
  ret void, !dbg !189 ; line:16 col:1
}

; Function Attrs: nounwind
define void @rpsl_M_hello_triangle_Fn_mainBreathing(%struct.texture* nocapture readonly %backbuffer, float %timeInSeconds) #0 {
entry:
  call void @___rpsl_block_marker(i32 0, i32 0, i32 0, i32 2, i32 -1, i32 0, i32 -1) #0
  %0 = alloca %struct.texture, align 8
  %backbufferDesc = alloca %struct.ResourceDesc, align 4
  %1 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 0
  %2 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 0
  %3 = load i32, i32* %2, align 4
  store i32 %3, i32* %1, align 8
  %4 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 1
  %5 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 1
  %6 = load i32, i32* %5, align 4
  store i32 %6, i32* %4, align 4
  %7 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 2
  %8 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 2
  %9 = load i32, i32* %8, align 4
  store i32 %9, i32* %7, align 8
  %10 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 3
  %11 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 3
  %12 = load i32, i32* %11, align 4
  store i32 %12, i32* %10, align 4
  %13 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 4, i32 0
  %14 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 4, i32 0
  %15 = load i16, i16* %14, align 2
  store i16 %15, i16* %13, align 8
  %16 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 4, i32 1
  %17 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 4, i32 1
  %18 = load i16, i16* %17, align 2
  store i16 %18, i16* %16, align 2
  %19 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 4, i32 2
  %20 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 4, i32 2
  %21 = load i32, i32* %20, align 4
  store i32 %21, i32* %19, align 4
  %22 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 4, i32 3
  %23 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 4, i32 3
  %24 = load i32, i32* %23, align 4
  store i32 %24, i32* %22, align 8
  %25 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 5
  %26 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 5
  %27 = load float, float* %26, align 4
  store float %27, float* %25, align 4
  %28 = getelementptr inbounds %struct.texture, %struct.texture* %0, i32 0, i32 6
  %29 = getelementptr inbounds %struct.texture, %struct.texture* %backbuffer, i32 0, i32 6
  %30 = load i32, i32* %29, align 4
  store i32 %30, i32* %28, align 8
  call void @llvm.dbg.value(metadata float %timeInSeconds, i64 0, metadata !190, metadata !157), !dbg !191 ; var:"timeInSeconds" !DIExpression() func:"mainBreathing"
  %31 = bitcast %struct.ResourceDesc* %backbufferDesc to i8*, !dbg !192 ; line:24 col:35
  call void @___rpsl_describe_handle(i8* %31, i32 36, i32* %1, i32 1) #0, !dbg !192 ; line:24 col:35
  %Width = getelementptr inbounds %struct.ResourceDesc, %struct.ResourceDesc* %backbufferDesc, i32 0, i32 3, !dbg !193 ; line:26 col:48
  %32 = load i32, i32* %Width, align 4, !dbg !193, !tbaa !162 ; line:26 col:48
  call void @llvm.dbg.value(metadata i32 %32, i64 0, metadata !194, metadata !157), !dbg !195 ; var:"width" !DIExpression() func:"mainBreathing"
  %Height = getelementptr inbounds %struct.ResourceDesc, %struct.ResourceDesc* %backbufferDesc, i32 0, i32 4, !dbg !196 ; line:27 col:38
  %33 = load i32, i32* %Height, align 4, !dbg !196, !tbaa !162 ; line:27 col:38
  call void @llvm.dbg.value(metadata i32 %33, i64 0, metadata !197, metadata !157), !dbg !198 ; var:"height" !DIExpression() func:"mainBreathing"
  %conv = uitofp i32 %33 to float, !dbg !199 ; line:29 col:32
  %conv1 = uitofp i32 %32 to float, !dbg !200 ; line:29 col:48
  %div = fdiv fast float %conv, %conv1, !dbg !201 ; line:29 col:39
  call void @llvm.dbg.value(metadata float %div, i64 0, metadata !202, metadata !157), !dbg !203 ; var:"oneOverAspectRatio" !DIExpression() func:"mainBreathing"
  call void @llvm.dbg.value(metadata <4 x float> <float 0.000000e+00, float 0x3FC99999A0000000, float 0x3FD99999A0000000, float 1.000000e+00>, i64 0, metadata !182, metadata !157), !dbg !204 ; var:"val" !DIExpression() func:"clear"
  %34 = alloca [2 x i8*], align 4, !dbg !206 ; line:294 col:12
  %.sub = getelementptr inbounds [2 x i8*], [2 x i8*]* %34, i32 0, i32 0
  %35 = bitcast i8** %.sub to %struct.texture**, !dbg !206 ; line:294 col:12
  store %struct.texture* %0, %struct.texture** %35, align 4, !dbg !206 ; line:294 col:12
  %36 = alloca <4 x float>, align 4, !dbg !206 ; line:294 col:12
  store <4 x float> <float 0.000000e+00, float 0x3FC99999A0000000, float 0x3FD99999A0000000, float 1.000000e+00>, <4 x float>* %36, align 4, !dbg !206 ; line:294 col:12
  %37 = getelementptr [2 x i8*], [2 x i8*]* %34, i32 0, i32 1, !dbg !206 ; line:294 col:12
  %38 = bitcast i8** %37 to <4 x float>**, !dbg !206 ; line:294 col:12
  store <4 x float>* %36, <4 x float>** %38, align 4, !dbg !206 ; line:294 col:12
  %39 = call i32 @___rpsl_node_call(i32 0, i32 2, i8** %.sub, i32 0, i32 0) #0, !dbg !206 ; line:294 col:12
  %40 = alloca [3 x i8*], align 4, !dbg !207 ; line:33 col:5
  %.sub.3 = getelementptr inbounds [3 x i8*], [3 x i8*]* %40, i32 0, i32 0
  %41 = bitcast i8** %.sub.3 to %struct.texture**, !dbg !207 ; line:33 col:5
  store %struct.texture* %0, %struct.texture** %41, align 4, !dbg !207 ; line:33 col:5
  %42 = alloca float, align 4, !dbg !207 ; line:33 col:5
  store float %div, float* %42, align 4, !dbg !207 ; line:33 col:5
  %43 = getelementptr [3 x i8*], [3 x i8*]* %40, i32 0, i32 1, !dbg !207 ; line:33 col:5
  %44 = bitcast i8** %43 to float**, !dbg !207 ; line:33 col:5
  store float* %42, float** %44, align 4, !dbg !207 ; line:33 col:5
  %45 = alloca float, align 4, !dbg !207 ; line:33 col:5
  store float %timeInSeconds, float* %45, align 4, !dbg !207 ; line:33 col:5
  %46 = getelementptr [3 x i8*], [3 x i8*]* %40, i32 0, i32 2, !dbg !207 ; line:33 col:5
  %47 = bitcast i8** %46 to float**, !dbg !207 ; line:33 col:5
  store float* %45, float** %47, align 4, !dbg !207 ; line:33 col:5
  %48 = call i32 @___rpsl_node_call(i32 2, i32 3, i8** %.sub.3, i32 0, i32 1) #0, !dbg !207 ; line:33 col:5
  ret void, !dbg !208 ; line:34 col:1
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.value(metadata, i64, metadata, metadata) #1

; Function Attrs: noinline nounwind
define dllexport i32 @___rps_dyn_lib_init(%0* nocapture readonly, i32) #2 {
body:
  %2 = load %0, %0* %0, align 4
  store %0 %2, %0* @s_rpslRuntimeProcs, align 16
  %not. = icmp ne i32 %1, 76
  %3 = sext i1 %not. to i32
  ret i32 %3
}

define i32 @___rpsl_node_call(i32, i32, i8**, i32, i32) {
body:
  %5 = load i32 (i32, i32, i8**, i32, i32)*, i32 (i32, i32, i8**, i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 1), align 4
  %6 = call i32 %5(i32 %0, i32 %1, i8** %2, i32 %3, i32 %4)
  ret i32 %6
}

define void @___rpsl_abort(i32) {
body:
  %1 = load void (i32)*, void (i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 0), align 16
  call void %1(i32 %0)
  ret void
}

define void @___rpsl_node_dependencies(i32, i32*, i32) {
body:
  %3 = load void (i32, i32*, i32)*, void (i32, i32*, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 2), align 8
  call void %3(i32 %0, i32* %1, i32 %2)
  ret void
}

define void @___rpsl_block_marker(i32, i32, i32, i32, i32, i32, i32) {
body:
  %7 = load void (i32, i32, i32, i32, i32, i32, i32)*, void (i32, i32, i32, i32, i32, i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 3), align 4
  call void %7(i32 %0, i32 %1, i32 %2, i32 %3, i32 %4, i32 %5, i32 %6)
  ret void
}

define void @___rpsl_scheduler_marker(i32, i32, i8*, i32) {
body:
  %4 = load void (i32, i32, i8*, i32)*, void (i32, i32, i8*, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 4), align 16
  call void %4(i32 %0, i32 %1, i8* %2, i32 %3)
  ret void
}

define void @___rpsl_describe_handle(i8*, i32, i32*, i32) {
body:
  %4 = load void (i8*, i32, i32*, i32)*, void (i8*, i32, i32*, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 5), align 4
  call void %4(i8* %0, i32 %1, i32* %2, i32 %3)
  ret void
}

define i32 @___rpsl_create_resource(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32) {
body:
  %11 = load i32 (i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)*, i32 (i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 6), align 8
  %12 = call i32 %11(i32 %0, i32 %1, i32 %2, i32 %3, i32 %4, i32 %5, i32 %6, i32 %7, i32 %8, i32 %9, i32 %10)
  ret i32 %12
}

define void @___rpsl_name_resource(i32, i8*, i32) {
body:
  %3 = load void (i32, i8*, i32)*, void (i32, i8*, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 7), align 4
  call void %3(i32 %0, i8* %1, i32 %2)
  ret void
}

define void @___rpsl_notify_out_param_resources(i32, i8*) {
body:
  %2 = load void (i32, i8*)*, void (i32, i8*)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 8), align 16
  call void %2(i32 %0, i8* %1)
  ret void
}

define i32 @___rpsl_dxop_unary_i32(i32, i32) {
body:
  %2 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 9), align 4
  %3 = call i32 %2(i32 %0, i32 %1)
  ret i32 %3
}

define i32 @___rpsl_dxop_binary_i32(i32, i32, i32) {
body:
  %3 = load i32 (i32, i32, i32)*, i32 (i32, i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 10), align 8
  %4 = call i32 %3(i32 %0, i32 %1, i32 %2)
  ret i32 %4
}

define i32 @___rpsl_dxop_tertiary_i32(i32, i32, i32, i32) {
body:
  %4 = load i32 (i32, i32, i32, i32)*, i32 (i32, i32, i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 11), align 4
  %5 = call i32 %4(i32 %0, i32 %1, i32 %2, i32 %3)
  ret i32 %5
}

define float @___rpsl_dxop_unary_f32(i32, float) {
body:
  %2 = load float (i32, float)*, float (i32, float)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 12), align 16
  %3 = call float %2(i32 %0, float %1)
  ret float %3
}

define float @___rpsl_dxop_binary_f32(i32, float, float) {
body:
  %3 = load float (i32, float, float)*, float (i32, float, float)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 13), align 4
  %4 = call float %3(i32 %0, float %1, float %2)
  ret float %4
}

define float @___rpsl_dxop_tertiary_f32(i32, float, float, float) {
body:
  %4 = load float (i32, float, float, float)*, float (i32, float, float, float)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 14), align 8
  %5 = call float %4(i32 %0, float %1, float %2, float %3)
  ret float %5
}

define i1 @___rpsl_dxop_isSpecialFloat_f32(i32, float) {
body:
  %2 = load i1 (i32, float)*, i1 (i32, float)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 15), align 4
  %3 = call i1 %2(i32 %0, float %1)
  ret i1 %3
}

define void @___rpsl_modify_pipeline(i32, i32, i32, i32, i8*) {
body:
  %5 = load void (i32, i32, i32, i32, i8*)*, void (i32, i32, i32, i32, i8*)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 16), align 16
  call void %5(i32 %0, i32 %1, i32 %2, i32 %3, i8* %4)
  ret void
}

define void @___rpsl_vertex_layout_append(i8*, i32, i32, i32, i32) {
body:
  %5 = load void (i8*, i32, i32, i32, i32)*, void (i8*, i32, i32, i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 17), align 4
  call void %5(i8* %0, i32 %1, i32 %2, i32 %3, i32 %4)
  ret void
}

define i8* @___rpsl_pso_get_field_ptr(i32, i32) {
body:
  %2 = load i8* (i32, i32)*, i8* (i32, i32)** getelementptr inbounds (%0, %0* @s_rpslRuntimeProcs, i32 0, i32 18), align 8
  %3 = call i8* %2(i32 %0, i32 %1)
  ret i8* %3
}

; Function Attrs: noinline nounwind
define void @rpsl_M_hello_triangle_Fn_main_wrapper(i32, i8** nocapture readonly, i32) #2 {
body:
  %3 = icmp eq i32 %0, 1, !dbg !209 ; line:11 col:0
  br i1 %3, label %trunk, label %err, !dbg !209 ; line:11 col:0

trunk:                                            ; preds = %body
  %4 = load i8*, i8** %1, align 4, !dbg !209 ; line:11 col:0
  %5 = alloca %struct.texture, align 8, !dbg !209 ; line:11 col:0
  %6 = and i32 %2, 1, !dbg !209 ; line:11 col:0
  %7 = icmp eq i32 %6, 0, !dbg !209 ; line:11 col:0
  %8 = bitcast i8* %4 to %struct.texture*, !dbg !209 ; line:11 col:0
  br i1 %7, label %.preheader, label %.loopexit, !dbg !209 ; line:11 col:0

err:                                              ; preds = %body
  call void @___rpsl_abort(i32 -3) #0, !dbg !209 ; line:11 col:0
  ret void, !dbg !209 ; line:11 col:0

.preheader:                                       ; preds = %trunk
  %9 = bitcast i8* %4 to %struct.ResourceDesc*, !dbg !209 ; line:11 col:0
  call fastcc void @"\01?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z"(%struct.texture* nonnull %5, i32 0, %struct.ResourceDesc* %9), !dbg !209 ; line:11 col:0
  br label %.loopexit, !dbg !209 ; line:11 col:0

.loopexit:                                        ; preds = %.preheader, %trunk
  %10 = phi %struct.texture* [ %8, %trunk ], [ %5, %.preheader ], !dbg !209 ; line:11 col:0
  call void @rpsl_M_hello_triangle_Fn_main(%struct.texture* %10), !dbg !209 ; line:11 col:0
  ret void
}

; Function Attrs: noinline nounwind
define void @rpsl_M_hello_triangle_Fn_mainBreathing_wrapper(i32, i8** nocapture readonly, i32) #2 {
body:
  %3 = icmp eq i32 %0, 2, !dbg !210 ; line:22 col:0
  br i1 %3, label %trunk, label %err, !dbg !210 ; line:22 col:0

trunk:                                            ; preds = %body
  %4 = load i8*, i8** %1, align 4, !dbg !210 ; line:22 col:0
  %5 = alloca %struct.texture, align 8, !dbg !210 ; line:22 col:0
  %6 = and i32 %2, 1, !dbg !210 ; line:22 col:0
  %7 = icmp eq i32 %6, 0, !dbg !210 ; line:22 col:0
  %8 = bitcast i8* %4 to %struct.texture*, !dbg !210 ; line:22 col:0
  br i1 %7, label %.preheader, label %.loopexit, !dbg !210 ; line:22 col:0

err:                                              ; preds = %body
  call void @___rpsl_abort(i32 -3) #0, !dbg !210 ; line:22 col:0
  ret void, !dbg !210 ; line:22 col:0

.preheader:                                       ; preds = %trunk
  %9 = bitcast i8* %4 to %struct.ResourceDesc*, !dbg !210 ; line:22 col:0
  call fastcc void @"\01?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z"(%struct.texture* nonnull %5, i32 0, %struct.ResourceDesc* %9), !dbg !210 ; line:22 col:0
  br label %.loopexit, !dbg !210 ; line:22 col:0

.loopexit:                                        ; preds = %.preheader, %trunk
  %10 = phi %struct.texture* [ %8, %trunk ], [ %5, %.preheader ], !dbg !210 ; line:22 col:0
  %11 = getelementptr i8*, i8** %1, i32 1, !dbg !210 ; line:22 col:0
  %12 = load i8*, i8** %11, align 4, !dbg !210 ; line:22 col:0
  %13 = bitcast i8* %12 to float*, !dbg !210 ; line:22 col:0
  %14 = load float, float* %13, align 4, !dbg !210 ; line:22 col:0
  call void @rpsl_M_hello_triangle_Fn_mainBreathing(%struct.texture* %10, float %14), !dbg !210 ; line:22 col:0
  ret void
}

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { noinline nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!105, !106}
!llvm.ident = !{!107}
!dx.source.contents = !{!108, !109}
!dx.source.defines = !{!2}
!dx.source.mainFileName = !{!110}
!dx.source.args = !{!111}
!dx.version = !{!112}
!dx.valver = !{!113}
!dx.shaderModel = !{!114}
!dx.typeAnnotations = !{!115, !139}
!dx.entryPoints = !{!151, !153, !155}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "dxc(private) 1.7.0.3789 (rps-merge-dxc1_7_2212_squash, b8686eeb8)", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, retainedTypes: !3, subprograms: !22, globals: !97)
!1 = !DIFile(filename: "C:\5C\5CUsers\5C\5CFlorian\5C\5Csource\5C\5Crepos\5C\5CRPS\5C\5Crender-pipeline-shaders\5C\5Cexamples\5C\5Chello_triangle.rpsl", directory: "")
!2 = !{}
!3 = !{!4, !6, !18, !10, !20}
!4 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint64_t", file: !1, line: 165, baseType: !5)
!5 = !DIBasicType(name: "uint64_t", size: 64, align: 64, encoding: DW_ATE_unsigned)
!6 = !DIDerivedType(tag: DW_TAG_typedef, name: "float4", file: !1, line: 14, baseType: !7)
!7 = !DICompositeType(tag: DW_TAG_class_type, name: "vector<float, 4>", file: !1, line: 14, size: 128, align: 32, elements: !8, templateParams: !14)
!8 = !{!9, !11, !12, !13}
!9 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !7, file: !1, line: 14, baseType: !10, size: 32, align: 32, flags: DIFlagPublic)
!10 = !DIBasicType(name: "float", size: 32, align: 32, encoding: DW_ATE_float)
!11 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !7, file: !1, line: 14, baseType: !10, size: 32, align: 32, offset: 32, flags: DIFlagPublic)
!12 = !DIDerivedType(tag: DW_TAG_member, name: "z", scope: !7, file: !1, line: 14, baseType: !10, size: 32, align: 32, offset: 64, flags: DIFlagPublic)
!13 = !DIDerivedType(tag: DW_TAG_member, name: "w", scope: !7, file: !1, line: 14, baseType: !10, size: 32, align: 32, offset: 96, flags: DIFlagPublic)
!14 = !{!15, !16}
!15 = !DITemplateTypeParameter(name: "element", type: !10)
!16 = !DITemplateValueParameter(name: "element_count", type: !17, value: i32 4)
!17 = !DIBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!18 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint32_t", file: !1, line: 26, baseType: !19)
!19 = !DIBasicType(name: "unsigned int", size: 32, align: 32, encoding: DW_ATE_unsigned)
!20 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint16_t", file: !1, line: 128, baseType: !21)
!21 = !DIBasicType(name: "uint16_t", size: 16, align: 16, encoding: DW_ATE_unsigned)
!22 = !{!23, !63, !78, !81, !84, !88, !91}
!23 = !DISubprogram(name: "make_default_texture_view_from_desc", linkageName: "\01?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z", scope: !24, file: !24, line: 152, type: !25, isLocal: false, isDefinition: true, scopeLine: 153, flags: DIFlagPrototyped, isOptimized: false, function: void (%struct.texture*, i32, %struct.ResourceDesc*)* @"\01?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z")
!24 = !DIFile(filename: "C:\5C\5CUsers\5C\5CFlorian\5C\5Csource\5C\5Crepos\5C\5CRPS\5C\5Crender-pipeline-shaders\5C\5Cexamples/___rpsl_builtin_header_.rpsl", directory: "")
!25 = !DISubroutineType(types: !26)
!26 = !{!27, !51, !52}
!27 = !DICompositeType(tag: DW_TAG_structure_type, name: "texture", file: !1, size: 288, align: 32, elements: !28)
!28 = !{!29, !30, !31, !32, !33, !40, !41, !42, !45, !48}
!29 = !DIDerivedType(tag: DW_TAG_member, name: "Resource", scope: !27, file: !1, baseType: !19, size: 32, align: 32)
!30 = !DIDerivedType(tag: DW_TAG_member, name: "ViewFormat", scope: !27, file: !1, baseType: !19, size: 32, align: 32, offset: 32)
!31 = !DIDerivedType(tag: DW_TAG_member, name: "TemporalLayer", scope: !27, file: !1, baseType: !19, size: 32, align: 32, offset: 64)
!32 = !DIDerivedType(tag: DW_TAG_member, name: "Flags", scope: !27, file: !1, baseType: !19, size: 32, align: 32, offset: 96)
!33 = !DIDerivedType(tag: DW_TAG_member, name: "SubresourceRange", scope: !27, file: !1, baseType: !34, size: 96, align: 32, offset: 128)
!34 = !DICompositeType(tag: DW_TAG_structure_type, name: "SubresourceRange", file: !1, size: 96, align: 32, elements: !35)
!35 = !{!36, !37, !38, !39}
!36 = !DIDerivedType(tag: DW_TAG_member, name: "base_mip_level", scope: !34, file: !1, baseType: !21, size: 16, align: 16)
!37 = !DIDerivedType(tag: DW_TAG_member, name: "mip_level_count", scope: !34, file: !1, baseType: !21, size: 16, align: 16, offset: 16)
!38 = !DIDerivedType(tag: DW_TAG_member, name: "base_array_layer", scope: !34, file: !1, baseType: !19, size: 32, align: 32, offset: 32)
!39 = !DIDerivedType(tag: DW_TAG_member, name: "array_layer_count", scope: !34, file: !1, baseType: !19, size: 32, align: 32, offset: 64)
!40 = !DIDerivedType(tag: DW_TAG_member, name: "MinLodClamp", scope: !27, file: !1, baseType: !10, size: 32, align: 32, offset: 224)
!41 = !DIDerivedType(tag: DW_TAG_member, name: "ComponentMapping", scope: !27, file: !1, baseType: !19, size: 32, align: 32, offset: 256)
!42 = !DISubprogram(name: "create1D", linkageName: "\01?create1D@texture@@SA?AU1@IIIIII@Z", scope: !27, file: !1, type: !43, isLocal: false, isDefinition: false, flags: DIFlagPrototyped, isOptimized: false)
!43 = !DISubroutineType(types: !44)
!44 = !{!27, !19, !19, !19, !19, !19, !19}
!45 = !DISubprogram(name: "create2D", linkageName: "\01?create2D@texture@@SA?AU1@IIIIIIIII@Z", scope: !27, file: !1, type: !46, isLocal: false, isDefinition: false, flags: DIFlagPrototyped, isOptimized: false)
!46 = !DISubroutineType(types: !47)
!47 = !{!27, !19, !19, !19, !19, !19, !19, !19, !19, !19}
!48 = !DISubprogram(name: "create3D", linkageName: "\01?create3D@texture@@SA?AU1@IIIIIII@Z", scope: !27, file: !1, type: !49, isLocal: false, isDefinition: false, flags: DIFlagPrototyped, isOptimized: false)
!49 = !DISubroutineType(types: !50)
!50 = !{!27, !19, !19, !19, !19, !19, !19, !19}
!51 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint", file: !1, baseType: !19)
!52 = !DICompositeType(tag: DW_TAG_structure_type, name: "ResourceDesc", file: !1, size: 288, align: 32, elements: !53)
!53 = !{!54, !55, !56, !57, !58, !59, !60, !61, !62}
!54 = !DIDerivedType(tag: DW_TAG_member, name: "Type", scope: !52, file: !1, baseType: !19, size: 32, align: 32)
!55 = !DIDerivedType(tag: DW_TAG_member, name: "TemporalLayers", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 32)
!56 = !DIDerivedType(tag: DW_TAG_member, name: "Flags", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 64)
!57 = !DIDerivedType(tag: DW_TAG_member, name: "Width", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 96)
!58 = !DIDerivedType(tag: DW_TAG_member, name: "Height", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 128)
!59 = !DIDerivedType(tag: DW_TAG_member, name: "DepthOrArraySize", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 160)
!60 = !DIDerivedType(tag: DW_TAG_member, name: "MipLevels", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 192)
!61 = !DIDerivedType(tag: DW_TAG_member, name: "Format", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 224)
!62 = !DIDerivedType(tag: DW_TAG_member, name: "SampleCount", scope: !52, file: !1, baseType: !19, size: 32, align: 32, offset: 256)
!63 = !DISubprogram(name: "make_default_buffer_view_from_desc", linkageName: "\01?make_default_buffer_view_from_desc@@YA?AUbuffer@@IUResourceDesc@@@Z", scope: !24, file: !24, line: 161, type: !64, isLocal: false, isDefinition: true, scopeLine: 162, flags: DIFlagPrototyped, isOptimized: false)
!64 = !DISubroutineType(types: !65)
!65 = !{!66, !51, !52}
!66 = !DICompositeType(tag: DW_TAG_structure_type, name: "buffer", file: !1, line: 159, size: 320, align: 64, elements: !67)
!67 = !{!68, !69, !70, !71, !72, !73, !74, !75}
!68 = !DIDerivedType(tag: DW_TAG_member, name: "Resource", scope: !66, file: !1, line: 159, baseType: !19, size: 32, align: 32)
!69 = !DIDerivedType(tag: DW_TAG_member, name: "ViewFormat", scope: !66, file: !1, line: 159, baseType: !19, size: 32, align: 32, offset: 32)
!70 = !DIDerivedType(tag: DW_TAG_member, name: "TemporalLayer", scope: !66, file: !1, line: 159, baseType: !19, size: 32, align: 32, offset: 64)
!71 = !DIDerivedType(tag: DW_TAG_member, name: "Flags", scope: !66, file: !1, line: 159, baseType: !19, size: 32, align: 32, offset: 96)
!72 = !DIDerivedType(tag: DW_TAG_member, name: "Offset", scope: !66, file: !1, line: 159, baseType: !5, size: 64, align: 64, offset: 128)
!73 = !DIDerivedType(tag: DW_TAG_member, name: "SizeInBytes", scope: !66, file: !1, line: 159, baseType: !5, size: 64, align: 64, offset: 192)
!74 = !DIDerivedType(tag: DW_TAG_member, name: "Stride", scope: !66, file: !1, line: 159, baseType: !19, size: 32, align: 32, offset: 256)
!75 = !DISubprogram(name: "create", linkageName: "\01?create@buffer@@SA?AU1@_KII@Z", scope: !66, file: !1, line: 159, type: !76, isLocal: false, isDefinition: false, scopeLine: 159, flags: DIFlagPrototyped, isOptimized: false)
!76 = !DISubroutineType(types: !77)
!77 = !{!66, !5, !19, !19}
!78 = !DISubprogram(name: "main", linkageName: "\01?main@@YAXUtexture@@@Z", scope: !1, file: !1, line: 11, type: !79, isLocal: false, isDefinition: true, scopeLine: 12, flags: DIFlagPrototyped, isOptimized: false, function: void (%struct.texture*)* @rpsl_M_hello_triangle_Fn_main)
!79 = !DISubroutineType(types: !80)
!80 = !{null, !27}
!81 = !DISubprogram(name: "mainBreathing", linkageName: "\01?mainBreathing@@YAXUtexture@@M@Z", scope: !1, file: !1, line: 22, type: !82, isLocal: false, isDefinition: true, scopeLine: 23, flags: DIFlagPrototyped, isOptimized: false, function: void (%struct.texture*, float)* @rpsl_M_hello_triangle_Fn_mainBreathing)
!82 = !DISubroutineType(types: !83)
!83 = !{null, !27, !10}
!84 = !DISubprogram(name: "make_default_texture_view", linkageName: "\01?make_default_texture_view@@YA?AUtexture@@IIII@Z", scope: !24, file: !24, line: 119, type: !85, isLocal: false, isDefinition: true, scopeLine: 120, flags: DIFlagPrototyped, isOptimized: false)
!85 = !DISubroutineType(types: !86)
!86 = !{!27, !51, !87, !51, !51}
!87 = !DIDerivedType(tag: DW_TAG_typedef, name: "RPS_FORMAT", file: !1, line: 34, baseType: !19)
!88 = !DISubprogram(name: "make_default_buffer_view", linkageName: "\01?make_default_buffer_view@@YA?AUbuffer@@I_K@Z", scope: !24, file: !24, line: 137, type: !89, isLocal: false, isDefinition: true, scopeLine: 138, flags: DIFlagPrototyped, isOptimized: false)
!89 = !DISubroutineType(types: !90)
!90 = !{!66, !51, !4}
!91 = !DISubprogram(name: "clear", linkageName: "\01?clear@@YA?AUnodeidentifier@@Utexture@@V?$vector@M$03@@@Z", scope: !24, file: !24, line: 292, type: !92, isLocal: false, isDefinition: true, scopeLine: 293, flags: DIFlagPrototyped, isOptimized: false)
!92 = !DISubroutineType(types: !93)
!93 = !{!94, !27, !6}
!94 = !DICompositeType(tag: DW_TAG_structure_type, name: "nodeidentifier", file: !24, line: 2, size: 32, align: 32, elements: !95)
!95 = !{!96}
!96 = !DIDerivedType(tag: DW_TAG_member, name: "unused", scope: !94, file: !24, line: 2, baseType: !51, size: 32, align: 32)
!97 = !{!98, !100, !101}
!98 = !DIGlobalVariable(name: "RPS_RESOURCE_TEX3D", scope: !0, file: !1, line: 157, type: !99, isLocal: true, isDefinition: true, variable: i32 4)
!99 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !19)
!100 = !DIGlobalVariable(name: "RPS_FORMAT_UNKNOWN", scope: !0, file: !1, line: 124, type: !99, isLocal: true, isDefinition: true, variable: i32 0)
!101 = !DIGlobalVariable(name: "___rpsl_pso_tmp", scope: !0, file: !24, line: 361, type: !102, isLocal: true, isDefinition: true)
!102 = !DICompositeType(tag: DW_TAG_structure_type, name: "Pipeline", file: !1, line: 361, size: 32, align: 32, elements: !103)
!103 = !{!104}
!104 = !DIDerivedType(tag: DW_TAG_member, name: "h", scope: !102, file: !1, line: 361, baseType: !17, size: 32, align: 32, flags: DIFlagPrivate)
!105 = !{i32 2, !"Dwarf Version", i32 4}
!106 = !{i32 2, !"Debug Info Version", i32 3}
!107 = !{!"dxc(private) 1.7.0.3789 (rps-merge-dxc1_7_2212_squash, b8686eeb8)"}
!108 = !{!"C:\5C\5CUsers\5C\5CFlorian\5C\5Csource\5C\5Crepos\5C\5CRPS\5C\5Crender-pipeline-shaders\5C\5Cexamples\5C\5Chello_triangle.rpsl", !"#include \22___rpsl_builtin_header_.rpsl\22\0A// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.\0D\0A//\0D\0A// This file is part of the AMD Render Pipeline Shaders SDK which is\0D\0A// released under the AMD INTERNAL EVALUATION LICENSE.\0D\0A//\0D\0A// See file LICENSE.txt for full license details.\0D\0A\0D\0Agraphics node Triangle([readwrite(rendertarget)] texture renderTarget : SV_Target0);\0D\0A\0D\0Aexport void main([readonly(present)] texture backbuffer)\0D\0A{\0D\0A    // clear and then render geometry to backbuffer\0D\0A    clear(backbuffer, float4(0.0, 0.2, 0.4, 1.0));\0D\0A    Triangle(backbuffer);\0D\0A}\0D\0A\0D\0A// ---------- Only Relevant for Tutorial Part 3 (Begin) ----------\0D\0Agraphics node TriangleBreathing([readwrite(rendertarget)] texture renderTarget\0D\0A                                : SV_Target0, float oneOverAspectRatio, float timeInSeconds);\0D\0A\0D\0Aexport void mainBreathing([readonly(present)] texture backbuffer, float timeInSeconds)\0D\0A{\0D\0A    ResourceDesc backbufferDesc = backbuffer.desc();\0D\0A\0D\0A    uint32_t width  = (uint32_t)backbufferDesc.Width;\0D\0A    uint32_t height = backbufferDesc.Height;\0D\0A\0D\0A    float oneOverAspectRatio = height / (float)width;\0D\0A\0D\0A    // clear and then render geometry to backbuffer\0D\0A    clear(backbuffer, float4(0.0, 0.2, 0.4, 1.0));\0D\0A    TriangleBreathing(backbuffer, oneOverAspectRatio, timeInSeconds);\0D\0A}\0D\0A\0D\0A// ---------- Only Relevant for Tutorial Part 3 (End) ------------"}
!109 = !{!"C:\5C\5CUsers\5C\5CFlorian\5C\5Csource\5C\5Crepos\5C\5CRPS\5C\5Crender-pipeline-shaders\5C\5Cexamples\5C___rpsl_builtin_header_.rpsl", !"\0Astruct nodeidentifier { uint unused; };\0A#define node nodeidentifier\0A\0Auint ___rpsl_asyncmarker();\0A#define async ___rpsl_asyncmarker();\0A\0Avoid ___rpsl_barrier();\0A#define sch_barrier ___rpsl_barrier\0A\0Avoid ___rpsl_subgraph_begin(uint flags, uint nameOffs, uint nameLen);\0Avoid ___rpsl_subgraph_end();\0A\0Avoid ___rpsl_abort(int errorCode);\0A\0A#define abort ___rpsl_abort\0A\0Atypedef int RpsBool;\0Atypedef RPS_FORMAT RpsFormat;\0A\0A// Syntax sugars\0A#define rtv         [readwrite(rendertarget)] texture\0A#define discard_rtv [writeonly(rendertarget)] texture\0A#define srv         [readonly(ps, cs)] texture\0A#define srv_buf     [readonly(ps, cs)] buffer\0A#define ps_srv      [readonly(ps)] texture\0A#define ps_srv_buf  [readonly(ps)] buffer\0A#define dsv         [readwrite(depth, stencil)] texture\0A#define uav         [readwrite(ps, cs)] texture\0A#define uav_buf     [readwrite(ps, cs)] buffer\0A\0Atexture ___rpsl_set_resource_name(texture h, uint nameOffset, uint nameLength);\0Abuffer ___rpsl_set_resource_name(buffer h, uint nameOffset, uint nameLength);\0A\0Astruct RpsViewport\0A{\0A    float x, y, width, height, minZ, maxZ;\0A};\0A\0Ainline RpsViewport viewport(float x, float y, float width, float height, float minZ = 0.0f, float maxZ = 1.0f)\0A{\0A    RpsViewport result = { x, y, width, height, minZ, maxZ };\0A    return result;\0A}\0A\0Ainline RpsViewport viewport(float width, float height)\0A{\0A    RpsViewport result = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };\0A    return result;\0A}\0A\0Ainline ResourceDesc     describe_resource      ( texture t ) { return t.desc(); }\0Ainline ResourceDesc     describe_resource      ( buffer b  ) { return b.desc(); }\0Ainline ResourceDesc     describe_texture       ( texture t ) { return t.desc(); }\0Ainline ResourceDesc     describe_buffer        ( buffer b  ) { return b.desc(); }\0A\0Auint             ___rpsl_create_resource( RPS_RESOURCE_TYPE  type,\0A                                          RPS_RESOURCE_FLAGS flags,\0A                                          RPS_FORMAT         format,\0A                                          uint               width,\0A                                          uint               height,\0A                                          uint               depthOrArraySize,\0A                                          uint               mipLevels,\0A                                          uint               sampleCount,\0A                                          uint               sampleQuality,\0A                                          uint               temporalLayers,\0A                                          uint               id = 0xFFFFFFFFu );\0A\0A// Built in nodes\0Atemplate<uint MaxRects>\0Agraphics node clear_color_regions( [readwrite(rendertarget, clear)] texture t, float4 data : SV_ClearColor, uint numRects, int4 rects[MaxRects] );\0Atemplate<uint MaxRects>\0Agraphics node clear_depth_stencil_regions( [readwrite(depth, stencil, clear)] texture t, RPS_CLEAR_FLAGS option, float d : SV_ClearDepth, uint s : SV_ClearStencil, uint numRects, int4 rects[MaxRects] );\0Atemplate<uint MaxRects>\0Acompute  node clear_texture_regions( [readwrite(clear)] texture t, uint4 data : SV_ClearColor, uint numRects, int4 rects[MaxRects] );\0A\0Agraphics node    clear_color            ( [writeonly(rendertarget, clear)] texture t, float4 data : SV_ClearColor );\0Agraphics node    clear_depth_stencil    ( [writeonly(depth, stencil, clear)] texture t, RPS_CLEAR_FLAGS option, float d : SV_ClearDepth, uint s : SV_ClearStencil );\0Acompute  node    clear_texture          ( [writeonly(clear)] texture t, uint4 data : SV_ClearColor );\0Acopy     node    clear_buffer           ( [writeonly(clear)] buffer b, uint4 data );\0Acopy     node    copy_texture           ( [readwrite(copy)] texture dst, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );\0Acopy     node    copy_buffer            ( [readwrite(copy)] buffer dst, uint64_t dstOffset, [readonly(copy)] buffer src, uint64_t srcOffset, uint64_t size );\0Acopy     node    copy_texture_to_buffer ( [readwrite(copy)] buffer dst, uint64_t dstByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );\0Acopy     node    copy_buffer_to_texture ( [readwrite(copy)] texture dst, uint3 dstOffset, [readonly(copy)] buffer src, uint64_t srcByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 srcOffset, uint3 extent );\0Agraphics node    resolve                ( [readwrite(resolve)] texture dst, uint2 dstOffset, [readonly(resolve)] texture src, uint2 srcOffset, uint2 extent, RPS_RESOLVE_MODE resolveMode );\0A\0Agraphics node    draw                   ( uint vertexCountPerInstance, uint instanceCount, uint startVertexLocation, uint startInstanceLocation );\0Agraphics node    draw_indexed           ( uint indexCountPerInstance, uint instanceCount, uint startIndexLocation, int baseVertexLocation, uint startInstanceLocation );\0Acompute  node    dispatch               ( uint3 numGroups );\0Acompute  node    dispatch_threads       ( uint3 numThreads );\0Agraphics node    draw_indirect          ( buffer args, uint64_t offset, uint32_t drawCount, uint32_t stride );\0Agraphics node    draw_indexed_indirect  ( buffer args, uint64_t offset, uint32_t drawCount, uint32_t stride );\0A\0Avoid             bind                   ( Pipeline pso );\0A\0Ainline uint ___rpsl_canonicalize_mip_level(uint inMipLevel, uint width, uint height = 1, uint depth = 1, uint sampleCount = 1)\0A{\0A    if (sampleCount > 1)\0A    {\0A        return 1;\0A    }\0A\0A    uint32_t w    = width;\0A    uint32_t h    = height;\0A    uint32_t d    = depth;\0A    uint32_t mips = 1;\0A\0A    while ((w > 1) || (h > 1) || (d > 1))\0A    {\0A        mips++;\0A        w = w >> 1;\0A        h = h >> 1;\0A        d = d >> 1;\0A    }\0A\0A    return (inMipLevel == 0) ? mips : min(inMipLevel, mips);\0A}\0A\0Ainline texture make_default_texture_view( uint resourceHdl, RPS_FORMAT format, uint arraySlices, uint numMips )\0A{\0A    texture result;\0A\0A    result.Resource = resourceHdl;\0A    result.ViewFormat = RPS_FORMAT_UNKNOWN;\0A    result.TemporalLayer = 0;\0A    result.Flags = 0;\0A    result.SubresourceRange.base_mip_level = 0;\0A    result.SubresourceRange.mip_level_count = uint16_t(numMips);\0A    result.SubresourceRange.base_array_layer = 0;\0A    result.SubresourceRange.array_layer_count = arraySlices;\0A    result.MinLodClamp = 0.0f;\0A    result.ComponentMapping = 0x3020100; // Default channel mapping\0A\0A    return result;\0A}\0A\0Ainline buffer make_default_buffer_view( uint resourceHdl, uint64_t size )\0A{\0A    buffer result;\0A\0A    result.Resource = resourceHdl;\0A    result.ViewFormat = 0;\0A    result.TemporalLayer = 0;\0A    result.Flags = 0;\0A    result.Offset = 0;\0A    result.SizeInBytes = size;\0A    result.Stride = 0;\0A\0A    return result;\0A}\0A\0Atexture make_default_texture_view_from_desc( uint resourceHdl, ResourceDesc desc )\0A{\0A    return make_default_texture_view(\0A        resourceHdl,\0A        desc.Format,\0A        (desc.Type == RPS_RESOURCE_TEX3D) ? 1 : desc.DepthOrArraySize,\0A        desc.MipLevels);\0A}\0A\0Abuffer make_default_buffer_view_from_desc( uint resourceHdl, ResourceDesc desc )\0A{\0A    return make_default_buffer_view(\0A        resourceHdl,\0A        (uint64_t(desc.Height) << 32u) | uint64_t(desc.Width));\0A}\0A\0Ainline texture create_texture( ResourceDesc desc )\0A{\0A    // TODO: Report error if type is not texture.\0A\0A    desc.MipLevels = ___rpsl_canonicalize_mip_level(desc.MipLevels,\0A                                                    desc.Width,\0A                                                    (desc.Type != RPS_RESOURCE_TEX1D) ? desc.Height : 1,\0A                                                    (desc.Type == RPS_RESOURCE_TEX3D) ? desc.DepthOrArraySize : 1,\0A                                                    desc.SampleCount);\0A\0A    uint resourceHdl = ___rpsl_create_resource( desc.Type, desc.Flags, desc.Format, desc.Width, desc.Height, desc.DepthOrArraySize, desc.MipLevels, desc.SampleCount, 0, desc.TemporalLayers );\0A\0A    uint arraySize = (desc.Type != RPS_RESOURCE_TEX3D) ? desc.DepthOrArraySize : 1;\0A\0A    return make_default_texture_view( resourceHdl, desc.Format, arraySize, desc.MipLevels );\0A}\0A\0Ainline buffer create_buffer( ResourceDesc desc )\0A{\0A    // TODO: Report error if type is not buffer.\0A\0A    uint resourceHdl = ___rpsl_create_resource( desc.Type, desc.Flags, desc.Format, desc.Width, desc.Height, desc.DepthOrArraySize, desc.MipLevels, desc.SampleCount, 0, desc.TemporalLayers );\0A\0A    uint64_t byteWidth = ((uint64_t)(desc.Height) << 32ULL) | ((uint64_t)(desc.Width));\0A\0A    return make_default_buffer_view( resourceHdl, byteWidth );\0A}\0A\0Ainline texture create_tex1d(\0A    RPS_FORMAT format,\0A    uint width,\0A    uint numMips = 1,\0A    uint arraySlices = 1,\0A    uint numTemporalLayers = 1,\0A    RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE )\0A{\0A    const RPS_RESOURCE_TYPE type = RPS_RESOURCE_TEX1D;\0A    const uint height = 1;\0A    const uint sampleCount = 1;\0A    const uint sampleQuality = 0;\0A\0A    numMips = ___rpsl_canonicalize_mip_level(numMips, width, height, 1, sampleCount);\0A\0A    uint resourceHdl = ___rpsl_create_resource( type, flags, format, width, height, arraySlices, numMips, sampleCount, sampleQuality, numTemporalLayers );\0A\0A    return make_default_texture_view( resourceHdl, format, arraySlices, numMips );\0A}\0A\0Ainline texture create_tex2d(\0A    RPS_FORMAT format,\0A    uint width,\0A    uint height,\0A    uint numMips = 1,\0A    uint arraySlices = 1,\0A    uint numTemporalLayers = 1,\0A    uint sampleCount = 1,\0A    uint sampleQuality = 0,\0A    RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE )\0A{\0A    const RPS_RESOURCE_TYPE type = RPS_RESOURCE_TEX2D;\0A\0A    numMips = ___rpsl_canonicalize_mip_level(numMips, width, height, 1, sampleCount);\0A\0A    const uint resourceHdl = ___rpsl_create_resource( type, flags, format, width, height, arraySlices, numMips, sampleCount, sampleQuality, numTemporalLayers );\0A\0A    return make_default_texture_view( resourceHdl, format, arraySlices, numMips );\0A}\0A\0Ainline texture create_tex3d(\0A    RPS_FORMAT format,\0A    uint width,\0A    uint height,\0A    uint depth,\0A    uint numMips = 1,\0A    uint numTemporalLayers = 1,\0A    RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE )\0A{\0A    const RPS_RESOURCE_TYPE type = RPS_RESOURCE_TEX3D;\0A    const uint sampleCount = 1;\0A    const uint sampleQuality = 0;\0A    const uint arraySlices = 1;\0A\0A    numMips = ___rpsl_canonicalize_mip_level(numMips, width, height, depth, sampleCount);\0A\0A    uint resourceHdl = ___rpsl_create_resource( type, flags, format, width, height, depth, numMips, sampleCount, sampleQuality, numTemporalLayers );\0A\0A    return make_default_texture_view( resourceHdl, format, arraySlices, numMips );\0A}\0A\0Ainline buffer create_buffer( uint64_t width, uint numTemporalLayers = 1, RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE )\0A{\0A    uint resourceHdl = ___rpsl_create_resource( RPS_RESOURCE_BUFFER,\0A                                              flags,\0A                                              RPS_FORMAT_UNKNOWN,\0A                                              (uint)width,\0A                                              (uint)(width >> 32ULL),\0A                                              1, 1, 1, 0, numTemporalLayers );\0A\0A    return make_default_buffer_view( resourceHdl, width );\0A}\0A\0Ainline texture create_texture_view(\0A    texture r,\0A    uint baseMip = 0,\0A    uint mipLevels = 1,\0A    uint baseArraySlice = 0,\0A    uint numArraySlices = 1,\0A    uint temporalLayer = 0,\0A    RPS_FORMAT format = RPS_FORMAT_UNKNOWN )\0A{\0A    return r.temporal(temporalLayer).mips(baseMip, mipLevels).array(baseArraySlice, numArraySlices).format(format);\0A}\0A\0Ainline buffer create_buffer_view(\0A    buffer r,\0A    uint64_t offset = 0,\0A    uint64_t sizeInBytes = 0,\0A    uint temporalLayer = 0,\0A    RPS_FORMAT format = RPS_FORMAT_UNKNOWN,\0A    uint structureStride = 0 )\0A{\0A    return r.temporal(temporalLayer).bytes(offset, sizeInBytes).format(format).stride(structureStride);\0A}\0A\0Ainline node clear( texture t, float4 val )\0A{\0A    return clear_color( t, val );\0A}\0A\0Ainline node clear( texture t, uint4 val )\0A{\0A    return clear_color( t, float4(val) );\0A}\0A\0Ainline node clear( texture t, float depth, uint stencil )\0A{\0A    return clear_depth_stencil( t, RPS_CLEAR_DEPTHSTENCIL, depth, stencil );\0A}\0A\0Ainline node clear_depth( texture t, float depth )\0A{\0A    return clear_depth_stencil( t, RPS_CLEAR_DEPTH, depth, 0 );\0A}\0A\0Ainline node clear_stencil( texture t, uint stencil )\0A{\0A    return clear_depth_stencil( t, RPS_CLEAR_STENCIL, 0.0f, stencil );\0A}\0A\0Atemplate<uint MaxRects>\0Ainline node clear_depth_regions( texture t, float depth, int4 rects[MaxRects] )\0A{\0A    return clear_depth_stencil_regions( t, RPS_CLEAR_DEPTH, depth, 0, MaxRects, rects );\0A}\0A\0Atemplate<uint MaxRects>\0Ainline node clear_stencil_regions( texture t, uint stencil, int4 rects[MaxRects] )\0A{\0A    return clear_depth_stencil_regions( t, RPS_CLEAR_STENCIL, 0.0f, stencil, MaxRects, rects );\0A}\0A\0Ainline node clear_uav( texture t, float4 val )\0A{\0A    return clear_texture( t, asuint(val) );\0A}\0A\0Ainline node clear_uav( texture t, uint4 val )\0A{\0A    return clear_texture( t, val );\0A}\0A\0Ainline node clear( buffer b, float4 val )\0A{\0A    return clear_buffer( b, asuint(val) );\0A}\0A\0Ainline node clear( buffer b, uint4 val )\0A{\0A    return clear_buffer( b, val );\0A}\0A\0Ainline node copy_texture( texture dst, texture src )\0A{\0A    return copy_texture( dst, uint3(0, 0, 0), src, uint3(0, 0, 0), uint3(0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU) );\0A}\0A\0Ainline node copy_buffer( buffer dst, buffer src )\0A{\0A    return copy_buffer( dst, 0, src, 0, 0xFFFFFFFFFFFFFFFFULL );\0A}\0A\0A// Experimental Pipeline states\0A\0Astatic Pipeline ___rpsl_pso_tmp;\0A\0Atemplate<typename T>\0Avoid ___rpsl_pso_modify_field(Pipeline p, uint i, T val);\0A\0Aenum RPS_DEPTH_WRITE_MASK {\0A  RPS_DEPTH_WRITE_MASK_ZERO,\0A  RPS_DEPTH_WRITE_MASK_ALL\0A};\0A\0Aenum RPS_COMPARISON_FUNC {\0A  RPS_COMPARISON_FUNC_NEVER,\0A  RPS_COMPARISON_FUNC_LESS,\0A  RPS_COMPARISON_FUNC_EQUAL,\0A  RPS_COMPARISON_FUNC_LESS_EQUAL,\0A  RPS_COMPARISON_FUNC_GREATER,\0A  RPS_COMPARISON_FUNC_NOT_EQUAL,\0A  RPS_COMPARISON_FUNC_GREATER_EQUAL,\0A  RPS_COMPARISON_FUNC_ALWAYS\0A};\0A\0Aenum RPS_STENCIL_OP {\0A  RPS_STENCIL_OP_KEEP,\0A  RPS_STENCIL_OP_ZERO,\0A  RPS_STENCIL_OP_REPLACE,\0A  RPS_STENCIL_OP_INCR_SAT,\0A  RPS_STENCIL_OP_DECR_SAT,\0A  RPS_STENCIL_OP_INVERT,\0A  RPS_STENCIL_OP_INCR,\0A  RPS_STENCIL_OP_DECR\0A};\0A\0Astruct RpsStencilOpDesc {\0A  RPS_STENCIL_OP      StencilFailOp;\0A  RPS_STENCIL_OP      StencilDepthFailOp;\0A  RPS_STENCIL_OP      StencilPassOp;\0A  RPS_COMPARISON_FUNC StencilFunc;\0A};\0A\0Astruct RpsDepthStencilDesc {\0A  RpsBool              DepthEnable;\0A  RPS_DEPTH_WRITE_MASK DepthWriteMask;\0A  RPS_COMPARISON_FUNC  DepthFunc;\0A  RpsBool              StencilEnable;\0A  uint                 StencilReadMask;\0A  uint                 StencilWriteMask;\0A  RpsStencilOpDesc     FrontFace;\0A  RpsStencilOpDesc     BackFace;\0A};\0A\0Aenum RPS_BLEND {\0A  RPS_BLEND_ZERO = 0,\0A  RPS_BLEND_ONE,\0A  RPS_BLEND_SRC_COLOR,\0A  RPS_BLEND_INV_SRC_COLOR,\0A  RPS_BLEND_SRC_ALPHA,\0A  RPS_BLEND_INV_SRC_ALPHA,\0A  RPS_BLEND_DEST_ALPHA,\0A  RPS_BLEND_INV_DEST_ALPHA,\0A  RPS_BLEND_DEST_COLOR,\0A  RPS_BLEND_INV_DEST_COLOR,\0A  RPS_BLEND_SRC_ALPHA_SAT,\0A  RPS_BLEND_BLEND_FACTOR,\0A  RPS_BLEND_INV_BLEND_FACTOR,\0A  RPS_BLEND_SRC1_COLOR,\0A  RPS_BLEND_INV_SRC1_COLOR,\0A  RPS_BLEND_SRC1_ALPHA,\0A  RPS_BLEND_INV_SRC1_ALPHA\0A};\0A\0Aenum RPS_BLEND_OP {\0A  RPS_BLEND_OP_ADD,\0A  RPS_BLEND_OP_SUBTRACT,\0A  RPS_BLEND_OP_REV_SUBTRACT,\0A  RPS_BLEND_OP_MIN,\0A  RPS_BLEND_OP_MAX\0A};\0A\0Aenum RPS_LOGIC_OP {\0A  RPS_LOGIC_OP_CLEAR = 0,\0A  RPS_LOGIC_OP_SET,\0A  RPS_LOGIC_OP_COPY,\0A  RPS_LOGIC_OP_COPY_INVERTED,\0A  RPS_LOGIC_OP_NOOP,\0A  RPS_LOGIC_OP_INVERT,\0A  RPS_LOGIC_OP_AND,\0A  RPS_LOGIC_OP_NAND,\0A  RPS_LOGIC_OP_OR,\0A  RPS_LOGIC_OP_NOR,\0A  RPS_LOGIC_OP_XOR,\0A  RPS_LOGIC_OP_EQUIV,\0A  RPS_LOGIC_OP_AND_REVERSE,\0A  RPS_LOGIC_OP_AND_INVERTED,\0A  RPS_LOGIC_OP_OR_REVERSE,\0A  RPS_LOGIC_OP_OR_INVERTED\0A};\0A\0Astruct RpsRenderTargetBlendDesc {\0A  RpsBool      BlendEnable;\0A  RpsBool      LogicOpEnable;\0A  RPS_BLEND    SrcBlend;\0A  RPS_BLEND    DestBlend;\0A  RPS_BLEND_OP BlendOp;\0A  RPS_BLEND    SrcBlendAlpha;\0A  RPS_BLEND    DestBlendAlpha;\0A  RPS_BLEND_OP BlendOpAlpha;\0A  RPS_LOGIC_OP LogicOp;\0A  uint         RenderTargetWriteMask;\0A};\0A\0Astruct RpsBlendDesc {\0A  RpsBool AlphaToCoverageEnable;\0A  RpsBool IndependentBlendEnable;\0A};\0A\0A#define per_vertex \22per_vertex\22\0A#define per_instance \22per_instance\22\0A\0ARpsVertexLayout vertex_layout(uint stride);\0A"}
!110 = !{!"C:\5C\5CUsers\5C\5CFlorian\5C\5Csource\5C\5Crepos\5C\5CRPS\5C\5Crender-pipeline-shaders\5C\5Cexamples\5C\5Chello_triangle.rpsl"}
!111 = !{!"-T", !"rps_6_2", !"-Vd", !"-default-linkage", !"external", !"-res_may_alias", !"-Zi", !"-Qembed_debug", !"-enable-16bit-types", !"-O3", !"-Wno-for-redefinition", !"-Wno-comma-in-init", !"-rps-module-name", !"hello_triangle", !"-HV", !"2021", !"-rps-target-dll"}
!112 = !{i32 1, i32 2}
!113 = !{i32 0, i32 0}
!114 = !{!"rps", i32 6, i32 2}
!115 = !{i32 0, %struct.texture undef, !116, %struct.SubresourceRange undef, !124, %struct.ResourceDesc undef, !129}
!116 = !{i32 36, !117, !118, !119, !120, !121, !122, !123}
!117 = !{i32 6, !"Resource", i32 3, i32 0, i32 7, i32 5}
!118 = !{i32 6, !"ViewFormat", i32 3, i32 4, i32 7, i32 5}
!119 = !{i32 6, !"TemporalLayer", i32 3, i32 8, i32 7, i32 5}
!120 = !{i32 6, !"Flags", i32 3, i32 12, i32 7, i32 5}
!121 = !{i32 6, !"SubresourceRange", i32 3, i32 16}
!122 = !{i32 6, !"MinLodClamp", i32 3, i32 28, i32 7, i32 9}
!123 = !{i32 6, !"ComponentMapping", i32 3, i32 32, i32 7, i32 5}
!124 = !{i32 12, !125, !126, !127, !128}
!125 = !{i32 6, !"base_mip_level", i32 3, i32 0, i32 7, i32 3}
!126 = !{i32 6, !"mip_level_count", i32 3, i32 2, i32 7, i32 3}
!127 = !{i32 6, !"base_array_layer", i32 3, i32 4, i32 7, i32 5}
!128 = !{i32 6, !"array_layer_count", i32 3, i32 8, i32 7, i32 5}
!129 = !{i32 36, !130, !131, !132, !133, !134, !135, !136, !137, !138}
!130 = !{i32 6, !"Type", i32 3, i32 0, i32 7, i32 5}
!131 = !{i32 6, !"TemporalLayers", i32 3, i32 4, i32 7, i32 5}
!132 = !{i32 6, !"Flags", i32 3, i32 8, i32 7, i32 5}
!133 = !{i32 6, !"Width", i32 3, i32 12, i32 7, i32 5}
!134 = !{i32 6, !"Height", i32 3, i32 16, i32 7, i32 5}
!135 = !{i32 6, !"DepthOrArraySize", i32 3, i32 20, i32 7, i32 5}
!136 = !{i32 6, !"MipLevels", i32 3, i32 24, i32 7, i32 5}
!137 = !{i32 6, !"Format", i32 3, i32 28, i32 7, i32 5}
!138 = !{i32 6, !"SampleCount", i32 3, i32 32, i32 7, i32 5}
!139 = !{i32 1, void (%struct.texture*, i32, %struct.ResourceDesc*)* @"\01?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z", !140, void (%struct.texture*)* @rpsl_M_hello_triangle_Fn_main, !145, void (%struct.texture*, float)* @rpsl_M_hello_triangle_Fn_mainBreathing, !148}
!140 = !{!141, !142, !143, !141}
!141 = !{i32 0, !2, !2}
!142 = !{i32 1, !2, !2}
!143 = !{i32 0, !144, !2}
!144 = !{i32 7, i32 5}
!145 = !{!142, !146}
!146 = !{i32 0, !147, !2}
!147 = !{i32 6, !"backbuffer"}
!148 = !{!142, !146, !149}
!149 = !{i32 0, !150, !2}
!150 = !{i32 6, !"timeInSeconds", i32 7, i32 9}
!151 = !{null, !"", null, null, !152}
!152 = !{i32 0, i64 8388640}
!153 = !{void (%struct.texture*)* @rpsl_M_hello_triangle_Fn_main, !"rpsl_M_hello_triangle_Fn_main", null, null, !154}
!154 = !{i32 8, i32 15}
!155 = !{void (%struct.texture*, float)* @rpsl_M_hello_triangle_Fn_mainBreathing, !"rpsl_M_hello_triangle_Fn_mainBreathing", null, null, !154}
!156 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "desc", arg: 2, scope: !23, file: !24, line: 152, type: !52)
!157 = !DIExpression()
!158 = !DILocation(line: 152, column: 77, scope: !23)
!159 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "resourceHdl", arg: 1, scope: !23, file: !24, line: 152, type: !51)
!160 = !DILocation(line: 152, column: 51, scope: !23)
!161 = !DILocation(line: 158, column: 14, scope: !23)
!162 = !{!163, !163, i64 0}
!163 = !{!"int", !164, i64 0}
!164 = !{!"omnipotent char", !165, i64 0}
!165 = !{!"Simple C/C++ TBAA"}
!166 = !DILocation(line: 157, column: 15, scope: !23)
!167 = !DILocation(line: 157, column: 20, scope: !23)
!168 = !DILocation(line: 157, column: 9, scope: !23)
!169 = !DILocation(line: 157, column: 54, scope: !23)
!170 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "numMips", arg: 4, scope: !84, file: !24, line: 119, type: !51)
!171 = !DILocation(line: 119, column: 103, scope: !84, inlinedAt: !172)
!172 = distinct !DILocation(line: 154, column: 12, scope: !23)
!173 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "arraySlices", arg: 3, scope: !84, file: !24, line: 119, type: !51)
!174 = !DILocation(line: 119, column: 85, scope: !84, inlinedAt: !172)
!175 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "resourceHdl", arg: 1, scope: !84, file: !24, line: 119, type: !51)
!176 = !DILocation(line: 119, column: 48, scope: !84, inlinedAt: !172)
!177 = !DILocation(line: 128, column: 56, scope: !84, inlinedAt: !172)
!178 = !DILocation(line: 134, column: 5, scope: !84, inlinedAt: !172)
!179 = !DILocation(line: 154, column: 5, scope: !23)
!180 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "backbuffer", arg: 1, scope: !78, file: !1, line: 11, type: !27)
!181 = !DILocation(line: 11, column: 46, scope: !78)
!182 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "val", arg: 2, scope: !91, file: !24, line: 292, type: !6)
!183 = !DILocation(line: 292, column: 38, scope: !91, inlinedAt: !184)
!184 = distinct !DILocation(line: 14, column: 5, scope: !78)
!185 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "t", arg: 1, scope: !91, file: !24, line: 292, type: !27)
!186 = !DILocation(line: 292, column: 28, scope: !91, inlinedAt: !184)
!187 = !DILocation(line: 294, column: 12, scope: !91, inlinedAt: !184)
!188 = !DILocation(line: 15, column: 5, scope: !78)
!189 = !DILocation(line: 16, column: 1, scope: !78)
!190 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "timeInSeconds", arg: 2, scope: !81, file: !1, line: 22, type: !10)
!191 = !DILocation(line: 22, column: 73, scope: !81)
!192 = !DILocation(line: 24, column: 35, scope: !81)
!193 = !DILocation(line: 26, column: 48, scope: !81)
!194 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "width", scope: !81, file: !1, line: 26, type: !18)
!195 = !DILocation(line: 26, column: 14, scope: !81)
!196 = !DILocation(line: 27, column: 38, scope: !81)
!197 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "height", scope: !81, file: !1, line: 27, type: !18)
!198 = !DILocation(line: 27, column: 14, scope: !81)
!199 = !DILocation(line: 29, column: 32, scope: !81)
!200 = !DILocation(line: 29, column: 48, scope: !81)
!201 = !DILocation(line: 29, column: 39, scope: !81)
!202 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "oneOverAspectRatio", scope: !81, file: !1, line: 29, type: !10)
!203 = !DILocation(line: 29, column: 11, scope: !81)
!204 = !DILocation(line: 292, column: 38, scope: !91, inlinedAt: !205)
!205 = distinct !DILocation(line: 32, column: 5, scope: !81)
!206 = !DILocation(line: 294, column: 12, scope: !91, inlinedAt: !205)
!207 = !DILocation(line: 33, column: 5, scope: !81)
!208 = !DILocation(line: 34, column: 1, scope: !81)
!209 = !DILocation(line: 11, scope: !78)
!210 = !DILocation(line: 22, scope: !81)
 