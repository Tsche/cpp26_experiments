template <typename T>
struct Null {
  Null() = default;
  explicit Null(auto&&...) {}

  T pop() {
    return {};
  }

  void push(auto&&) { }
};