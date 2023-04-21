(module
  (memory 1 1 shared)
    (func (export "test1") (result i64)
      ;; where    expected mem val   replacement  op
      ;; i32.const 0 i64.const 0        i64.const 1 i64.atomic.rmw.cmpxchg

      i32.const 0 i64.const 1   i64.atomic.rmw.xchg
    )

    ;; (func (export "test1") (result i64)
    ;;   i32.const 0
    ;;   i32.const 0 i64.atomic.load
    ;;   i64.atomic.store
    ;;   i64.const 0
    ;; )
)
(assert_return (invoke "test1") (i64.const 0))