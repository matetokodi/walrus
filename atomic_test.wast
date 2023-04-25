(module
  (memory 1 1 shared)
    (func (export "test1") (result i64)
      ;; where    expected mem val   replacement  op
      ;; i32.const 0 i64.const 0        i64.const 1 i64.atomic.rmw.cmpxchg

      i32.const 0 i64.const 1   i64.atomic.rmw.xchg drop
      i32.const 0 i64.const 2   i64.atomic.rmw.xchg
      ;; i64.const 3 i64.const 2 i64.add
    )

    ;; (func (export "test1") (result i64)
    ;;   i32.const 0
    ;;   i32.const 0 i64.atomic.load
    ;;   i64.atomic.store
    ;;   i64.const 0
    ;; )
)
(assert_return (invoke "test1") (i64.const 1))
;; (assert_return (invoke "test1") (i64.const 5))
