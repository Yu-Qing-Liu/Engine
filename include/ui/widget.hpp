class Widget {
  public:
	Widget() = default;
	Widget(Widget &&) = default;
	Widget(const Widget &) = default;
	Widget &operator=(Widget &&) = default;
	Widget &operator=(const Widget &) = default;
	~Widget() = default;

  private:
};
