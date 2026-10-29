.pragma library

// 标签条横向滚动的纯计算：内容宽 / 可视宽 / 偏移夹取 / 滚轮步进 / 拖动位移换算。
// 这里只做数值运算，不访问任何 QML 对象，便于独立断言边界（见 build-agent/ev3/tagstrip_math_test.js）。

// 可视区之外还剩多少宽度可滚：内容比视口窄时为 0
function maxOffset(contentWidth, viewWidth) {
    return Math.max(0, contentWidth - viewWidth);
}

// 偏移量一律夹在 [-maxOffset, 0]：左端不出现空白，右端不越过内容末尾
function clampOffset(offset, contentWidth, viewWidth) {
    return Math.min(0, Math.max(-maxOffset(contentWidth, viewWidth), offset));
}

// 滚轮步进：在当前偏移上叠加增量再夹紧
function wheelOffset(current, delta, contentWidth, viewWidth) {
    return clampOffset(current + delta, contentWidth, viewWidth);
}

// 拖动位移换算：按下时的偏移 + 指针水平位移，再夹紧
function dragOffset(pressOffset, deltaX, contentWidth, viewWidth) {
    return clampOffset(pressOffset + deltaX, contentWidth, viewWidth);
}
