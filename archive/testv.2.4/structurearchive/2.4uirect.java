struct Rect {
    int x, y, w, h;

    bool Contains(int px, int py) const {
        return px >= x && px <= x + w &&
               py >= y && py <= y + h;
    }
};

Rect R(int x, int y, int w, int h) {
    return { x, y, w, h };
}

##
Rect r	>>> UiRect r
Rect row   >>>	UiRect row
Rect R(...) >>>	UiRect R(...)