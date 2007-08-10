struct vec
{
    union
    {
        struct { float x, y, z; };
        float v[3];
    };

    vec() {}
    vec(float a, float b, float c) : x(a), y(b), z(c) {}
    vec(float *v) : x(v[0]), y(v[1]), z(v[2]) {}

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }

    bool iszero() const { return x==0 && y==0 && z==0; }

    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }

    vec &mul(float f) { x *= f; y *= f; z *= f; return *this; }
    vec &div(float f) { x /= f; y /= f; z /= f; return *this; }
    vec &add(float f) { x += f; y += f; z += f; return *this; }
    vec &sub(float f) { x -= f; y -= f; z -= f; return *this; }

    vec &add(const vec &o) { x += o.x; y += o.y; z += o.z; return *this; }
    vec &sub(const vec &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

    float squaredlen() const { return x*x + y*y + z*z; }
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; }

    float magnitude() const { return sqrtf(squaredlen()); }
    vec &normalize() { div(magnitude()); return *this; }

    float dist(const vec &e) const { vec t; return dist(e, t); }
    float dist(const vec &e, vec &t) const { t = *this; t.sub(e); return t.magnitude(); }

    float distxy(const vec &e) const { float dx = e.x - x, dy = e.y - y; return sqrtf(dx*dx + dy*dy); }

    bool reject(const vec &o, float max) const { return x>o.x+max || x<o.x-max || y>o.y+max || y<o.y-max; }
};

struct ivec
{
    union
    {
        struct { int x, y, z; };
        int v[3];
    };

    ivec() {}
    ivec(const vec &v) : x(int(v.x)), y(int(v.y)), z(int(v.z)) {}
    ivec(int a, int b, int c) : x(a), y(b), z(c) {}

    vec tovec() const { return vec(x, y, z); }

    int &operator[](int i)       { return v[i]; }
    int  operator[](int i) const { return v[i]; }

    bool operator==(const ivec &v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const ivec &v) const { return x!=v.x || y!=v.y || z!=v.z; }
    ivec &mul(int n) { x *= n; y *= n; z *= n; return *this; }
    ivec &div(int n) { x /= n; y /= n; z /= n; return *this; }
    ivec &add(int n) { x += n; y += n; z += n; return *this; }
    ivec &sub(int n) { x -= n; y -= n; z -= n; return *this; }
    ivec &add(const ivec &v) { x += v.x; y += v.y; z += v.z; return *this; }
    ivec &sub(const ivec &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    ivec &mask(int n) { x &= n; y &= n; z &= n; return *this; }
    ivec &cross(const ivec &a, const ivec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }
    int dot(const ivec &o) const { return x*o.x + y*o.y + z*o.z; }
};

