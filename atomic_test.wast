(module
  (memory 1 1 shared)
    ;; (func (export "test1") (result i64)
      ;; where    expected mem val   replacement  op
      ;; i32.const 0 i64.const 0        i64.const 1 i64.atomic.rmw.cmpxchg

      ;; i32.const 0 i64.const 1   i64.atomic.rmw.xchg drop
      ;; i32.const 0 i64.const 2   i64.atomic.rmw.xchg
      ;; i64.const 3 i64.const 2 i64.add
    ;; )

    ;; (func (export "test1") (result i64)
    ;;   i32.const 0
    ;;   i32.const 0 i64.atomic.load
    ;;   i64.atomic.store
    ;;   i64.const 0
    ;; )

     (func (export "add") (result i64)
       i32.const 0 i64.const 100 i64.atomic.rmw.add drop
       i32.const 0 i64.atomic.load
     )

     (func (export "sub") (result i64)
       i32.const 0 i64.const 99 i64.atomic.rmw.sub drop
       i32.const 0 i64.atomic.load
     )

     (func (export "and") (result i64)
       i32.const 0 i64.const 2 i64.atomic.rmw.and drop
       i32.const 0 i64.atomic.load
     )

     (func (export "or") (result i64)
       i32.const 0 i64.const 4 i64.atomic.rmw.or drop
       i32.const 0 i64.atomic.load
     )

     (func (export "xor") (result i64)
       i32.const 0 i64.const 2 i64.atomic.rmw.xor drop
       i32.const 0 i64.atomic.load
     )

     (func (export "xchg") (result i64)
       i32.const 0 i64.const 48 i64.atomic.rmw.xchg
     )

     (func (export "cmpxchg") (result i64)
       i32.const 0 i64.const 48 i64.const 64 i64.atomic.rmw.cmpxchg
     )

     (func (export "load") (result i64)
       i32.const 0 i64.atomic.load
     )

     (func (export "store") (result i64)
       i32.const 0 i64.const 200 i64.atomic.store
       i32.const 0 i64.atomic.load
     )

     (func (export "add32") (result i32)
       i32.const 0 i32.const 100 i32.atomic.rmw.add drop
       i32.const 0 i32.atomic.load
     )

     (func (export "sub32") (result i32)
       i32.const 0 i32.const 99 i32.atomic.rmw.sub drop
       i32.const 0 i32.atomic.load
     )

     (func (export "and32") (result i32)
       i32.const 0 i32.const 2 i32.atomic.rmw.and drop
       i32.const 0 i32.atomic.load
     )

     (func (export "or32") (result i32)
       i32.const 0 i32.const 4 i32.atomic.rmw.or drop
       i32.const 0 i32.atomic.load
     )

     (func (export "xor32") (result i32)
       i32.const 0 i32.const 2 i32.atomic.rmw.xor drop
       i32.const 0 i32.atomic.load
     )

     (func (export "xchg32") (result i32)
       i32.const 0 i32.const 48 i32.atomic.rmw.xchg
     )

     (func (export "cmpxchg32") (result i32)
       i32.const 0 i32.const 48 i32.const 64 i32.atomic.rmw.cmpxchg
     )

     (func (export "load32") (result i32)
       i32.const 0 i32.atomic.load
     )

     (func (export "store32") (result i32)
       i32.const 0 i32.const 200 i32.atomic.store
       i32.const 0 i32.atomic.load
     )
)
;; (assert_return (invoke "test1") (i64.const 1))
;; (assert_return (invoke "test1") (i64.const 5))

;; (assert_return (invoke "add32") (i32.const 100))
;; (assert_return (invoke "sub32") (i32.const 1))
;; (assert_return (invoke "and32") (i32.const 0))
;; (assert_return (invoke "or32") (i32.const 4))
;; (assert_return (invoke "xor32") (i32.const 6))
;; (assert_return (invoke "xchg32") (i32.const 6))
;; (assert_return (invoke "cmpxchg32") (i32.const 48))
;; (assert_return (invoke "load32") (i32.const 64))
;; (assert_return (invoke "store32") (i32.const 200))

(assert_return (invoke "add") (i64.const 100))
(assert_return (invoke "sub") (i64.const 1))
(assert_return (invoke "and") (i64.const 0))
(assert_return (invoke "or") (i64.const 4))
(assert_return (invoke "xor") (i64.const 6))
(assert_return (invoke "xchg") (i64.const 6))
;; (assert_return (invoke "cmpxchg") (i64.const 48))
(assert_return (invoke "load") (i64.const 64))
(assert_return (invoke "store") (i64.const 200))
