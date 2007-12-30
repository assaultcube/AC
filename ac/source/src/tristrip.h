VAR(dbgts, 0, 0, 1);
VAR(tsswap, 0, 1, 1);

struct tristrip
{
    enum
    {
        // must be larger than all other triangle/vert indices
        UNUSED  = 0xFFFE,
        REMOVED = 0xFFFF
    };

    enum
    {
        // must be larger than all other vert indices
        RESTART = 0xFFFE,
        LIST    = 0xFFFF
    };

    struct triangle
    {
        ushort v[3], n[3];

        bool link(ushort neighbor, ushort old = UNUSED)
        {
            loopi(3)
            {
                if(n[i]==neighbor) return true;
                else if(n[i]==old) { n[i] = neighbor; return true; }
            }
            if(dbgts && old==UNUSED) conoutf("excessive links");
            return false;
        }    
 
        void unlink(ushort neighbor, ushort unused = UNUSED)
        {
            loopi(3) if(n[i]==neighbor) n[i] = unused;
        }
 
        int numlinks() const { int num = 0; loopi(3) if(n[i]<UNUSED) num++; return num; }

        bool hasvert(ushort idx) const { loopi(3) if(v[i]==idx) return true; return false; }
        ushort diffvert(ushort v1, ushort v2) { loopi(3) if(v[i]!=v1 && v[i]!=v2) return v[i]; return UNUSED; }

        bool haslink(ushort neighbor) const { loopi(3) if(n[i]==neighbor) return true; return false; }
    };

    vector<triangle> triangles;
    vector<ushort> connectivity[4];
    vector<uchar> nodes;

    void addtriangles(const ushort *triidxs, int numtris)
    {
        if(dbgts) conoutf("addtriangles: tris = %d, inds = %d", numtris, numtris*3);
        loopi(numtris)
        {
            triangle &tri = triangles.add();
            loopj(3) 
            {
                tri.v[j] = *triidxs++;
                tri.n[j] = UNUSED;
            }
            if(tri.v[0]==tri.v[1] || tri.v[1]==tri.v[2] || tri.v[2]==tri.v[0])
            {
                if(dbgts) conoutf("degenerate triangle");
                triangles.drop();
            }
        }
    }

    struct edge { ushort from, to; };

    void findconnectivity()
    {
        hashtable<edge, ushort> edges;
        nodes.setsizenodelete(0);
        loopv(triangles)
        {
            triangle &tri = triangles[i];
            loopj(3)
            {
                edge e = { tri.v[j==2 ? 0 : j+1], tri.v[j] };
                ushort owner = i;
                edges.access(e, &owner);

                while(nodes.length()<=tri.v[j]) nodes.add(0);
                nodes[tri.v[j]]++;
            }
        }
        loopv(triangles)
        {
            triangle &tri = triangles[i];
            loopj(3)
            {
                edge e = { tri.v[j], tri.v[j==2 ? 0 : j+1] };
                ushort *owner = edges.access(e);
                if(!owner) continue;
                else if(!tri.link(*owner))
                {
                    if(dbgts) conoutf("failed linkage 1: %d -> %d", *owner, i);
                }
                else if(!triangles[*owner].link(i))
                {
                    if(dbgts) conoutf("failed linkage 2: %d -> %d", *owner, i);
                    tri.unlink(*owner);
                }
            }
        }
        loopi(4) connectivity[i].setsizenodelete(0);
        loopv(triangles) connectivity[triangles[i].numlinks()].add(i);
        if(dbgts) conoutf("no connections: %d", connectivity[0].length());
    }

    void removeconnectivity(ushort i)
    {
        triangle &tri = triangles[i];
        loopj(3) if(tri.n[j]<UNUSED)
        {
            triangle &neighbor = triangles[tri.n[j]];
            int conn = neighbor.numlinks();
            bool linked = false;
            loopk(3) if(neighbor.n[k]==i) { linked = true; neighbor.n[k] = REMOVED; break; }
            if(linked)
            {
                connectivity[conn].replacewithlast(tri.n[j]);
                connectivity[conn-1].add(tri.n[j]);
            }
        }
        removenodes(i);
    }

    bool remaining() const
    {
        loopi(4) if(!connectivity[i].empty()) return true;
        return false;
    }

    ushort leastconnected()
    {
        loopi(4) if(!connectivity[i].empty()) 
        { 
            ushort least = connectivity[i].pop();
            removeconnectivity(least);
            return least;
        }
        return UNUSED;
    }

    int findedge(const triangle &from, const triangle &to, ushort v = UNUSED)
    {
        loopi(3) loopj(3)
        {
            if(from.v[i]==to.v[j] && from.v[i]!=v) return i;
        }
        return -1;
    }

    void removenodes(int i)
    {
        loopj(3) nodes[triangles[i].v[j]]--;
    }

    ushort nexttriangle(triangle &tri, bool &nextswap, ushort v1 = UNUSED, ushort v2 = UNUSED)
    {
        ushort next = UNUSED;
        int nextscore = 777;
        nextswap = false;
        loopi(3) if(tri.n[i]<UNUSED)
        {
            triangle &nexttri = triangles[tri.n[i]];
            int score = nexttri.numlinks();
            bool swap = false; 
            if(v1!=UNUSED) 
            {
                if(!nexttri.hasvert(v1))
                {
                    swap = true;
                    score += nodes[v2] > nodes[v1] ? 1 : -1;
                }
                else if(nexttri.hasvert(v2)) continue;
                else score += nodes[v1] > nodes[v2] ? 1 : -1;
                if(!tsswap && swap) continue;
                score += swap ? 1 : -1;
            }
            if(score < nextscore) { next = tri.n[i]; nextswap = swap; nextscore = score; }
        }
        if(next!=UNUSED) 
        {
            tri.unlink(next, REMOVED);
            connectivity[triangles[next].numlinks()].replacewithlast(next);
            removeconnectivity(next);
        }
        return next;
    }

    void buildstrip(vector<ushort> &strip, bool reverse = false, bool prims = false)
    {
        ushort prev = leastconnected();
        if(prev==UNUSED) return;
        triangle &first = triangles[prev];
        bool doswap;
        ushort cur = nexttriangle(first, doswap);
        if(cur==UNUSED)
        {
            loopi(3) strip.add(first.v[!prims && reverse && i>=1 ? 3-i : i]);
            return;
        }
        int from = findedge(first, triangles[cur]), 
            to = findedge(first, triangles[cur], first.v[from]);
        if(from+1!=to) swap(from, to); 
        strip.add(first.v[(to+1)%3]);
        if(reverse) swap(from, to);
        strip.add(first.v[from]);
        strip.add(first.v[to]);

        ushort v1 = first.v[to], v2 = first.v[from];
        while(cur!=UNUSED)
        {
            prev = cur;
            cur = nexttriangle(triangles[prev], doswap, v1, v2);
            if(doswap) strip.add(v2);
            ushort v = triangles[prev].diffvert(v1, v2);
            strip.add(v);
            if(!doswap) v2 = v1;
            v1 = v;
        }

    }

    void buildstrips(vector<ushort> &strips, bool prims = true, bool degen = false)
    {
        vector<ushort> singles;
        findconnectivity();
        int numtris = 0, numstrips = 0;
        while(remaining())
        {
            vector<ushort> strip;
            bool reverse = degen && !strips.empty() && (strips.length()&1);
            buildstrip(strip, reverse, prims);
            numstrips++;
            numtris += strip.length()-2;
            if(strip.length()==3 && prims)
            {
                loopv(strip) singles.add(strip[i]);
                continue;
            }
            if(!strips.empty())
            {
                if(degen) { strips.dup(); strips.add(strip[0]); }
                else strips.add(RESTART);
            }
            loopv(strip) strips.add(strip[i]);
        }
        if(prims && !singles.empty())
        {
            strips.add(LIST);
            loopv(singles) strips.add(singles[i]);
        }
        if(dbgts) conoutf("strips = %d, tris = %d, inds = %d, merges = %d", numstrips, numtris, numtris + numstrips*2, (degen ? 2 : 1)*(numstrips-1));
    }

};

static inline uint hthash(const tristrip::edge &x) { return x.from^x.to; }
static inline bool htcmp(const tristrip::edge &x, const tristrip::edge &y) { return x.from==y.from && x.to==y.to; }
            
