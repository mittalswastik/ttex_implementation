; ModuleID = 'ext.ll'
source_filename = "test_code.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%"class.std::ios_base::Init" = type { i8 }
%struct.ident_t = type { i32, i32, i32, i32, i8* }
%"class.std::basic_ostream" = type { i32 (...)**, %"class.std::basic_ios" }
%"class.std::basic_ios" = type { %"class.std::ios_base", %"class.std::basic_ostream"*, i8, i8, %"class.std::basic_streambuf"*, %"class.std::ctype"*, %"class.std::num_put"*, %"class.std::num_get"* }
%"class.std::ios_base" = type { i32 (...)**, i64, i64, i32, i32, i32, %"struct.std::ios_base::_Callback_list"*, %"struct.std::ios_base::_Words", [8 x %"struct.std::ios_base::_Words"], i32, %"struct.std::ios_base::_Words"*, %"class.std::locale" }
%"struct.std::ios_base::_Callback_list" = type { %"struct.std::ios_base::_Callback_list"*, void (i32, %"class.std::ios_base"*, i32)*, i32, i32 }
%"struct.std::ios_base::_Words" = type { i8*, i64 }
%"class.std::locale" = type { %"class.std::locale::_Impl"* }
%"class.std::locale::_Impl" = type { i32, %"class.std::locale::facet"**, i64, %"class.std::locale::facet"**, i8** }
%"class.std::locale::facet" = type <{ i32 (...)**, i32, [4 x i8] }>
%"class.std::basic_streambuf" = type { i32 (...)**, i8*, i8*, i8*, i8*, i8*, i8*, %"class.std::locale" }
%"class.std::ctype" = type <{ %"class.std::locale::facet.base", [4 x i8], %struct.__locale_struct*, i8, [7 x i8], i32*, i32*, i16*, i8, [256 x i8], [256 x i8], i8, [6 x i8] }>
%"class.std::locale::facet.base" = type <{ i32 (...)**, i32 }>
%struct.__locale_struct = type { [13 x %struct.__locale_data*], i16*, i32*, i32*, [13 x i8*] }
%struct.__locale_data = type opaque
%"class.std::num_put" = type { %"class.std::locale::facet.base", [4 x i8] }
%"class.std::num_get" = type { %"class.std::locale::facet.base", [4 x i8] }

$__clang_call_terminate = comdat any

@_ZStL8__ioinit = internal global %"class.std::ios_base::Init" zeroinitializer, align 1
@__dso_handle = external hidden global i8
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 514, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
@_ZSt4cout = external dso_local global %"class.std::basic_ostream", align 8
@.str = private unnamed_addr constant [40 x i8] c"checking the number of times it prints \00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 66, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @0, i32 0, i32 0) }, align 8
@.gomp_critical_user_.var = common global [8 x i32] zeroinitializer
@.str.1 = private unnamed_addr constant [6 x i8] c"Sum: \00", align 1
@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @_GLOBAL__sub_I_test_code.cpp, i8* null }]

declare dso_local void @_ZNSt8ios_base4InitC1Ev(%"class.std::ios_base::Init"* noundef) unnamed_addr #0

; Function Attrs: nounwind
declare dso_local void @_ZNSt8ios_base4InitD1Ev(%"class.std::ios_base::Init"* noundef) unnamed_addr #1

; Function Attrs: nofree nounwind
declare dso_local i32 @__cxa_atexit(void (i8*)*, i8*, i8*) local_unnamed_addr #2

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() local_unnamed_addr #3 {
entry:
  %array = alloca [30 x i32], align 16
  %sum = alloca i32, align 4
  %j = alloca i32, align 4
  %upper_bound = alloca i32, align 4
  %0 = bitcast [30 x i32]* %array to i8*
  call void @llvm.lifetime.start.p0i8(i64 120, i8* nonnull %0) #6
  %1 = bitcast [30 x i32]* %array to <4 x i32>*
  store <4 x i32> <i32 1, i32 2, i32 3, i32 4>, <4 x i32>* %1, align 16, !tbaa !4
  %arrayidx.4 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 4
  %2 = bitcast i32* %arrayidx.4 to <4 x i32>*
  store <4 x i32> <i32 5, i32 6, i32 7, i32 8>, <4 x i32>* %2, align 16, !tbaa !4
  %arrayidx.8 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 8
  %3 = bitcast i32* %arrayidx.8 to <4 x i32>*
  store <4 x i32> <i32 9, i32 10, i32 11, i32 12>, <4 x i32>* %3, align 16, !tbaa !4
  %arrayidx.12 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 12
  %4 = bitcast i32* %arrayidx.12 to <4 x i32>*
  store <4 x i32> <i32 13, i32 14, i32 15, i32 16>, <4 x i32>* %4, align 16, !tbaa !4
  %arrayidx.16 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 16
  %5 = bitcast i32* %arrayidx.16 to <4 x i32>*
  store <4 x i32> <i32 17, i32 18, i32 19, i32 20>, <4 x i32>* %5, align 16, !tbaa !4
  %arrayidx.20 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 20
  %6 = bitcast i32* %arrayidx.20 to <4 x i32>*
  store <4 x i32> <i32 21, i32 22, i32 23, i32 24>, <4 x i32>* %6, align 16, !tbaa !4
  %arrayidx.24 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 24
  %7 = bitcast i32* %arrayidx.24 to <4 x i32>*
  store <4 x i32> <i32 25, i32 26, i32 27, i32 28>, <4 x i32>* %7, align 16, !tbaa !4
  %arrayidx.28 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 28
  store i32 29, i32* %arrayidx.28, align 16, !tbaa !4
  %arrayidx.29 = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 29
  store i32 30, i32* %arrayidx.29, align 4, !tbaa !4
  %8 = bitcast i32* %sum to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %8) #6
  store i32 0, i32* %sum, align 4, !tbaa !4
  %9 = bitcast i32* %j to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %9) #6
  store i32 20, i32* %j, align 4, !tbaa !4
  %10 = bitcast i32* %upper_bound to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %10) #6
  store i32 10, i32* %upper_bound, align 4, !tbaa !4
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @3, i32 4, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, i32*, i32*, [30 x i32]*, i32*)* @.omp_outlined. to void (i32*, i32*, ...)*), i32 1, i32* nonnull %upper_bound, i32* nonnull %j, [30 x i32]* nonnull %array, i32* nonnull %sum)
  %call1.i = call noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l(%"class.std::basic_ostream"* noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, i8* noundef nonnull getelementptr inbounds ([6 x i8], [6 x i8]* @.str.1, i64 0, i64 0), i64 noundef 5)
  %11 = load i32, i32* %sum, align 4, !tbaa !4
  %call1 = call noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSolsEi(%"class.std::basic_ostream"* noundef nonnull @_ZSt4cout, i32 noundef %11)
  %12 = bitcast %"class.std::basic_ostream"* %call1 to i8**
  %vtable.i = load i8*, i8** %12, align 8, !tbaa !8
  %vbase.offset.ptr.i = getelementptr i8, i8* %vtable.i, i64 -24
  %13 = bitcast i8* %vbase.offset.ptr.i to i64*
  %vbase.offset.i = load i64, i64* %13, align 8
  %14 = bitcast %"class.std::basic_ostream"* %call1 to i8*
  %add.ptr.i = getelementptr inbounds i8, i8* %14, i64 %vbase.offset.i
  %_M_ctype.i.i = getelementptr inbounds i8, i8* %add.ptr.i, i64 240
  %15 = bitcast i8* %_M_ctype.i.i to %"class.std::ctype"**
  %16 = load %"class.std::ctype"*, %"class.std::ctype"** %15, align 8, !tbaa !10
  %tobool.not.i.i.i = icmp eq %"class.std::ctype"* %16, null
  br i1 %tobool.not.i.i.i, label %if.then.i.i.i, label %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i

if.then.i.i.i:                                    ; preds = %entry
  call void @_ZSt16__throw_bad_castv() #11
  unreachable

_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i: ; preds = %entry
  %_M_widen_ok.i.i.i = getelementptr inbounds %"class.std::ctype", %"class.std::ctype"* %16, i64 0, i32 8
  %17 = load i8, i8* %_M_widen_ok.i.i.i, align 8, !tbaa !14
  %tobool.not.i3.i.i = icmp eq i8 %17, 0
  br i1 %tobool.not.i3.i.i, label %if.end.i.i.i, label %if.then.i4.i.i

if.then.i4.i.i:                                   ; preds = %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i
  %arrayidx.i.i.i = getelementptr inbounds %"class.std::ctype", %"class.std::ctype"* %16, i64 0, i32 9, i64 10
  %18 = load i8, i8* %arrayidx.i.i.i, align 1, !tbaa !16
  br label %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit

if.end.i.i.i:                                     ; preds = %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i
  call void @_ZNKSt5ctypeIcE13_M_widen_initEv(%"class.std::ctype"* noundef nonnull %16)
  %19 = bitcast %"class.std::ctype"* %16 to i8 (%"class.std::ctype"*, i8)***
  %vtable.i.i.i = load i8 (%"class.std::ctype"*, i8)**, i8 (%"class.std::ctype"*, i8)*** %19, align 8, !tbaa !8
  %vfn.i.i.i = getelementptr inbounds i8 (%"class.std::ctype"*, i8)*, i8 (%"class.std::ctype"*, i8)** %vtable.i.i.i, i64 6
  %20 = load i8 (%"class.std::ctype"*, i8)*, i8 (%"class.std::ctype"*, i8)** %vfn.i.i.i, align 8
  %call.i.i.i = call noundef signext i8 %20(%"class.std::ctype"* noundef nonnull %16, i8 noundef signext 10)
  br label %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit

_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit: ; preds = %if.end.i.i.i, %if.then.i4.i.i
  %retval.0.i.i.i = phi i8 [ %18, %if.then.i4.i.i ], [ %call.i.i.i, %if.end.i.i.i ]
  %call1.i8 = call noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSo3putEc(%"class.std::basic_ostream"* noundef nonnull %call1, i8 noundef signext %retval.0.i.i.i)
  %call.i.i9 = call noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSo5flushEv(%"class.std::basic_ostream"* noundef nonnull %call1.i8)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %10) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %9) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %8) #6
  call void @llvm.lifetime.end.p0i8(i64 120, i8* nonnull %0) #6
  ret i32 0
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #4

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #4

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias nocapture noundef readonly %.global_tid., i32* noalias nocapture noundef readnone %.bound_tid., i32* nocapture noundef nonnull readonly align 4 dereferenceable(4) %upper_bound, i32* nocapture noundef nonnull align 4 dereferenceable(4) %j, [30 x i32]* nocapture noundef nonnull readonly align 4 dereferenceable(120) %array, i32* nocapture noundef nonnull align 4 dereferenceable(4) %sum) #5 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) !ttex_array !17 !ttex_sub_array !18 !parallel_id !19 {
entry:
  %.omp.lb = alloca i32, align 4
  %.omp.ub = alloca i32, align 4
  %.omp.stride = alloca i32, align 4
  %.omp.is_last = alloca i32, align 4
  %0 = load i32, i32* %upper_bound, align 4, !tbaa !4
  %sub2 = add nsw i32 %0, -1
  %cmp = icmp sgt i32 %0, 0
  br i1 %cmp, label %omp.precond.then, label %entry.omp.precond.end_crit_edge

entry.omp.precond.end_crit_edge:                  ; preds = %entry
  %.pre = load i32, i32* %.global_tid., align 4, !tbaa !4
  br label %omp.precond.end

omp.precond.then:                                 ; preds = %entry
  %1 = bitcast i32* %.omp.lb to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #6
  store i32 0, i32* %.omp.lb, align 4, !tbaa !4
  %2 = bitcast i32* %.omp.ub to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #6
  store i32 %sub2, i32* %.omp.ub, align 4, !tbaa !4
  %3 = bitcast i32* %.omp.stride to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %3) #6
  store i32 1, i32* %.omp.stride, align 4, !tbaa !4
  %4 = bitcast i32* %.omp.is_last to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %4) #6
  store i32 0, i32* %.omp.is_last, align 4, !tbaa !4
  %5 = load i32, i32* %.global_tid., align 4, !tbaa !4
  call void @__kmpc_for_static_init_4(%struct.ident_t* nonnull @1, i32 %5, i32 34, i32* nonnull %.omp.is_last, i32* nonnull %.omp.lb, i32* nonnull %.omp.ub, i32* nonnull %.omp.stride, i32 1, i32 1, i32 1) #6
  %6 = load i32, i32* %.omp.ub, align 4, !tbaa !4
  %cmp4 = icmp sgt i32 %6, %sub2
  %cond = select i1 %cmp4, i32 %sub2, i32 %6
  store i32 %cond, i32* %.omp.ub, align 4, !tbaa !4
  %7 = load i32, i32* %.omp.lb, align 4, !tbaa !4
  %cmp5.not58 = icmp sgt i32 %7, %cond
  br i1 %cmp5.not58, label %omp.loop.exit, label %omp.inner.for.body.preheader

omp.inner.for.body.preheader:                     ; preds = %omp.precond.then
  %8 = sext i32 %7 to i64
  br label %omp.inner.for.body

omp.inner.for.body:                               ; preds = %invoke.cont10, %omp.inner.for.body.preheader
  %indvars.iv = phi i64 [ %8, %omp.inner.for.body.preheader ], [ %indvars.iv.next, %invoke.cont10 ]
  %local_sum.060 = phi i32 [ 0, %omp.inner.for.body.preheader ], [ %add6, %invoke.cont10 ]
  %call = call i32 @omp_get_thread_num()
  %arrayidx = getelementptr inbounds [30 x i32], [30 x i32]* %array, i64 0, i64 %indvars.iv
  %9 = load i32, i32* %arrayidx, align 4, !tbaa !4
  %add6 = add nsw i32 %9, %local_sum.060
  %10 = load i32, i32* %j, align 4, !tbaa !4
  %dec = add nsw i32 %10, -1
  store i32 %dec, i32* %j, align 4, !tbaa !4
  %call1.i49 = invoke noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l(%"class.std::basic_ostream"* noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, i8* noundef nonnull getelementptr inbounds ([40 x i8], [40 x i8]* @.str, i64 0, i64 0), i64 noundef 39)
          to label %invoke.cont8 unwind label %lpad7.loopexit

invoke.cont8:                                     ; preds = %SplitLoopHeader, %omp.inner.for.body
  %vtable.i = load i8*, i8** bitcast (%"class.std::basic_ostream"* @_ZSt4cout to i8**), align 8, !tbaa !8
  %vbase.offset.ptr.i = getelementptr i8, i8* %vtable.i, i64 -24
  %11 = bitcast i8* %vbase.offset.ptr.i to i64*
  %vbase.offset.i = load i64, i64* %11, align 8
  %_M_ctype.i.i = getelementptr inbounds i8, i8* bitcast (%"class.std::basic_streambuf"** getelementptr inbounds (%"class.std::basic_ostream", %"class.std::basic_ostream"* @_ZSt4cout, i64 0, i32 1, i32 4) to i8*), i64 %vbase.offset.i
  %12 = bitcast i8* %_M_ctype.i.i to %"class.std::ctype"**
  %13 = load %"class.std::ctype"*, %"class.std::ctype"** %12, align 8, !tbaa !10
  %tobool.not.i.i.i = icmp eq %"class.std::ctype"* %13, null
  br i1 %tobool.not.i.i.i, label %if.then.i.i.i, label %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i

if.then.i.i.i:                                    ; preds = %invoke.cont8
  invoke void @_ZSt16__throw_bad_castv() #11
          to label %.noexc unwind label %lpad7.loopexit.split-lp

.noexc:                                           ; preds = %if.then.i.i.i
  unreachable

_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i: ; preds = %invoke.cont8
  %_M_widen_ok.i.i.i = getelementptr inbounds %"class.std::ctype", %"class.std::ctype"* %13, i64 0, i32 8
  %14 = load i8, i8* %_M_widen_ok.i.i.i, align 8, !tbaa !14
  %tobool.not.i3.i.i = icmp eq i8 %14, 0
  br i1 %tobool.not.i3.i.i, label %if.end.i.i.i, label %if.then.i4.i.i

if.then.i4.i.i:                                   ; preds = %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i
  %arrayidx.i.i.i = getelementptr inbounds %"class.std::ctype", %"class.std::ctype"* %13, i64 0, i32 9, i64 10
  %15 = load i8, i8* %arrayidx.i.i.i, align 1, !tbaa !16
  br label %_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc.exit.i

if.end.i.i.i:                                     ; preds = %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i
  invoke void @_ZNKSt5ctypeIcE13_M_widen_initEv(%"class.std::ctype"* noundef nonnull %13)
          to label %.noexc52 unwind label %lpad7.loopexit

.noexc52:                                         ; preds = %if.end.i.i.i
  %16 = bitcast %"class.std::ctype"* %13 to i8 (%"class.std::ctype"*, i8)***
  %vtable.i.i.i = load i8 (%"class.std::ctype"*, i8)**, i8 (%"class.std::ctype"*, i8)*** %16, align 8, !tbaa !8
  %vfn.i.i.i = getelementptr inbounds i8 (%"class.std::ctype"*, i8)*, i8 (%"class.std::ctype"*, i8)** %vtable.i.i.i, i64 6
  %17 = load i8 (%"class.std::ctype"*, i8)*, i8 (%"class.std::ctype"*, i8)** %vfn.i.i.i, align 8
  %call.i.i.i53 = invoke noundef signext i8 %17(%"class.std::ctype"* noundef nonnull %13, i8 noundef signext 10)
          to label %_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc.exit.i unwind label %lpad7.loopexit

_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc.exit.i: ; preds = %.noexc52, %if.then.i4.i.i
  %retval.0.i.i.i = phi i8 [ %15, %if.then.i4.i.i ], [ %call.i.i.i53, %.noexc52 ]
  %call1.i54 = invoke noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSo3putEc(%"class.std::basic_ostream"* noundef nonnull @_ZSt4cout, i8 noundef signext %retval.0.i.i.i)
          to label %call1.i.noexc unwind label %lpad7.loopexit

call1.i.noexc:                                    ; preds = %_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc.exit.i
  br label %SplitLoopLatch

invoke.cont10:                                    ; preds = %CheckOverShoot, %SplitLoopLatch
  %indvars.iv.next = add nsw i64 %indvars.iv, 2
  %18 = load i32, i32* %.omp.ub, align 4, !tbaa !4
  %19 = sext i32 %18 to i64
  %cmp5.not.not = icmp slt i64 %indvars.iv, %19
  br i1 %cmp5.not.not, label %omp.inner.for.body, label %omp.loop.exit.loopexit

lpad7.loopexit:                                   ; preds = %_ZNKSt9basic_iosIcSt11char_traitsIcEE5widenEc.exit.i, %.noexc52, %if.end.i.i.i, %omp.inner.for.body
  %lpad.loopexit = landingpad { i8*, i32 }
          catch i8* null
  br label %lpad7

lpad7.loopexit.split-lp:                          ; preds = %if.then.i.i.i
  %lpad.loopexit.split-lp = landingpad { i8*, i32 }
          catch i8* null
  br label %lpad7

lpad7:                                            ; preds = %lpad7.loopexit.split-lp, %lpad7.loopexit
  %lpad.phi = phi { i8*, i32 } [ %lpad.loopexit, %lpad7.loopexit ], [ %lpad.loopexit.split-lp, %lpad7.loopexit.split-lp ]
  %20 = extractvalue { i8*, i32 } %lpad.phi, 0
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %4) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #6
  call void @__clang_call_terminate(i8* %20) #12
  unreachable

omp.loop.exit.loopexit:                           ; preds = %invoke.cont10
  %add6.lcssa2 = phi i32 [ %add6, %invoke.cont10 ]
  br label %omp.loop.exit

omp.loop.exit:                                    ; preds = %omp.loop.exit.loopexit, %omp.precond.then
  %local_sum.0.lcssa = phi i32 [ 0, %omp.precond.then ], [ %add6.lcssa2, %omp.loop.exit.loopexit ]
  call void @__kmpc_for_static_fini(%struct.ident_t* nonnull @1, i32 %5)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %4) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #6
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #6
  br label %omp.precond.end

omp.precond.end:                                  ; preds = %omp.loop.exit, %entry.omp.precond.end_crit_edge
  %21 = phi i32 [ %5, %omp.loop.exit ], [ %.pre, %entry.omp.precond.end_crit_edge ]
  %local_sum.1 = phi i32 [ %local_sum.0.lcssa, %omp.loop.exit ], [ 0, %entry.omp.precond.end_crit_edge ]
  call void @__kmpc_barrier(%struct.ident_t* nonnull @2, i32 %21)
  call void @__kmpc_critical(%struct.ident_t* nonnull @3, i32 %21, [8 x i32]* nonnull @.gomp_critical_user_.var)
  %22 = load i32, i32* %sum, align 4, !tbaa !4
  %add21 = add nsw i32 %22, %local_sum.1
  store i32 %add21, i32* %sum, align 4, !tbaa !4
  call void @__kmpc_end_critical(%struct.ident_t* nonnull @3, i32 %21, [8 x i32]* nonnull @.gomp_critical_user_.var)
  ret void

SplitLoopPreheader:                               ; No predecessors!
  %iterator = alloca i64, align 8
  %iterator_bound = alloca i64, align 8
  store i64 %indvars.iv, i64* %iterator, align 8
  %23 = load i32, i32* %.omp.ub, align 4
  %value_sext = sext i32 %23 to i64
  store i64 %value_sext, i64* %iterator_bound, align 8
  br label %SplitLoopHeader

SplitLoopHeader:                                  ; preds = %CheckOverShoot, %SplitLoopPreheader
  %24 = phi i64 [ %indvars.iv, %SplitLoopPreheader ], [ %increment, %CheckOverShoot ]
  br label %invoke.cont8

SplitLoopLatch:                                   ; preds = %call1.i.noexc
  %increment = add nsw i64 %24, 1
  %store_start = load i64, i64* %iterator_bound, align 8
  %25 = icmp sle i64 %increment, %store_start
  br i1 %25, label %CheckOverShoot, label %invoke.cont10

CheckOverShoot:                                   ; preds = %SplitLoopLatch
  %26 = add nsw i64 1, %24
  %27 = icmp sle i64 %increment, %26
  br i1 %27, label %SplitLoopHeader, label %invoke.cont10
}

declare dso_local void @__kmpc_for_static_init_4(%struct.ident_t*, i32, i32, i32*, i32*, i32*, i32*, i32, i32, i32) local_unnamed_addr

; Function Attrs: nounwind
declare dso_local i32 @omp_get_thread_num() local_unnamed_addr #1

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare void @__kmpc_for_static_fini(%struct.ident_t*, i32) local_unnamed_addr #6

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8* %0) local_unnamed_addr #7 comdat {
  %2 = tail call i8* @__cxa_begin_catch(i8* %0) #6
  tail call void @_ZSt9terminatev() #12
  unreachable
}

declare dso_local i8* @__cxa_begin_catch(i8*) local_unnamed_addr

declare dso_local void @_ZSt9terminatev() local_unnamed_addr

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(%struct.ident_t*, i32) local_unnamed_addr #8

; Function Attrs: convergent nounwind
declare void @__kmpc_end_critical(%struct.ident_t*, i32, [8 x i32]*) local_unnamed_addr #8

; Function Attrs: convergent nounwind
declare void @__kmpc_critical(%struct.ident_t*, i32, [8 x i32]*) local_unnamed_addr #8

; Function Attrs: nounwind
declare !callback !20 void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr #6

declare dso_local noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSolsEi(%"class.std::basic_ostream"* noundef, i32 noundef) local_unnamed_addr #0

declare dso_local noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l(%"class.std::basic_ostream"* noundef nonnull align 8 dereferenceable(8), i8* noundef, i64 noundef) local_unnamed_addr #0

declare dso_local noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSo3putEc(%"class.std::basic_ostream"* noundef, i8 noundef signext) local_unnamed_addr #0

declare dso_local noundef nonnull align 8 dereferenceable(8) %"class.std::basic_ostream"* @_ZNSo5flushEv(%"class.std::basic_ostream"* noundef) local_unnamed_addr #0

; Function Attrs: noreturn
declare dso_local void @_ZSt16__throw_bad_castv() local_unnamed_addr #9

declare dso_local void @_ZNKSt5ctypeIcE13_M_widen_initEv(%"class.std::ctype"* noundef) local_unnamed_addr #0

; Function Attrs: uwtable
define internal void @_GLOBAL__sub_I_test_code.cpp() #10 section ".text.startup" {
entry:
  tail call void @_ZNSt8ios_base4InitC1Ev(%"class.std::ios_base::Init"* noundef nonnull @_ZStL8__ioinit)
  %0 = tail call i32 @__cxa_atexit(void (i8*)* bitcast (void (%"class.std::ios_base::Init"*)* @_ZNSt8ios_base4InitD1Ev to void (i8*)*), i8* getelementptr inbounds (%"class.std::ios_base::Init", %"class.std::ios_base::Init"* @_ZStL8__ioinit, i64 0, i32 0), i8* nonnull @__dso_handle) #6
  ret void
}

attributes #0 = { "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nofree nounwind }
attributes #3 = { mustprogress norecurse uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { argmemonly nofree nosync nounwind willreturn }
attributes #5 = { alwaysinline norecurse nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { noinline noreturn nounwind }
attributes #8 = { convergent nounwind }
attributes #9 = { noreturn "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #11 = { noreturn }
attributes #12 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}
!nvvm.annotations = !{}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 14.0.0 (https://github.ncsu.edu/smittal6/ttex_implementation.git ed376897764464880cd392fd4a1c297ea9bc6d24)"}
!4 = !{!5, !5, i64 0}
!5 = !{!"int", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C++ TBAA"}
!8 = !{!9, !9, i64 0}
!9 = !{!"vtable pointer", !7, i64 0}
!10 = !{!11, !12, i64 240}
!11 = !{!"_ZTSSt9basic_iosIcSt11char_traitsIcEE", !12, i64 216, !6, i64 224, !13, i64 225, !12, i64 232, !12, i64 240, !12, i64 248, !12, i64 256}
!12 = !{!"any pointer", !6, i64 0}
!13 = !{!"bool", !6, i64 0}
!14 = !{!15, !6, i64 56}
!15 = !{!"_ZTSSt5ctypeIcE", !12, i64 16, !13, i64 24, !12, i64 32, !12, i64 40, !12, i64 48, !6, i64 56, !6, i64 57, !6, i64 313, !6, i64 569}
!16 = !{!6, !6, i64 0}
!17 = !{[1 x i32] [i32 -1]}
!18 = !{[1 x i32] [i32 1]}
!19 = !{i32 1}
!20 = !{!21}
!21 = !{i64 2, i64 -1, i64 -1, i1 true}
