// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  // 指向字符（char）类型的指针，用于存储锁的名称。
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};
// spinlock 是一种互斥锁的实现方式，也被称为自旋锁。
// 它的名称来源于其工作方式，即在尝试获取锁时，线程会循环（自旋）等待，不断地检查锁的状态，直到成功获取锁为止。
