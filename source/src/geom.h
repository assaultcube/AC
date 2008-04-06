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

    vec &cross(const vec &a, const vec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }

    void rotate_around_z(float angle) { *this = vec(cosf(angle)*x-sinf(angle)*y, cosf(angle)*y+sinf(angle)*x, z); }
    void rotate_around_x(float angle) { *this = vec(x, cosf(angle)*y-sinf(angle)*z, cosf(angle)*z+sinf(angle)*y); }
    void rotate_around_y(float angle) { *this = vec(cosf(angle)*x-sinf(angle)*z, y, cosf(angle)*z+sinf(angle)*x); }

    vec &rotate(float angle, const vec &d)
    {
        float c = cosf(angle), s = sinf(angle);
        return rotate(c, s, d);
    }

    vec &rotate(float c, float s, const vec &d)
    {
        *this = vec(x*(d.x*d.x*(1-c)+c) + y*(d.x*d.y*(1-c)-d.z*s) + z*(d.x*d.z*(1-c)+d.y*s),
                    x*(d.y*d.x*(1-c)+d.z*s) + y*(d.y*d.y*(1-c)+c) + z*(d.y*d.z*(1-c)-d.x*s),
                    x*(d.x*d.z*(1-c)-d.y*s) + y*(d.y*d.z*(1-c)+d.x*s) + z*(d.z*d.z*(1-c)+c));
        return *this;
    }

    void orthogonal(const vec &d)
    {
        int i = fabs(d.x) > fabs(d.y) ? (fabs(d.x) > fabs(d.z) ? 0 : 2) : (fabs(d.y) > fabs(d.z) ? 1 : 2);
        v[i] = d[(i+1)%3];
        v[(i+1)%3] = -d[i];
        v[(i+2)%3] = 0;
    }
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

