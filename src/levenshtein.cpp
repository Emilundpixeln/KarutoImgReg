#include <vector>
#include "levenshtein.h"


// from https://gist.github.com/TheRayTracer/2644387
std::vector<size_t> d;

size_t levenshtein_distance(std::string_view s, std::string_view t)
{
    size_t n = s.size();
    size_t m = t.size();

    ++n; ++m;
    if (d.size() < n * m)
        d.resize(n * m);


    d[0] = 0;
    for (unsigned int i = 1; i < m; ++i) d[i * n] = i;
    for (unsigned int i = 1; i < n; ++i) d[i] = i;

    for (size_t i = 1, im = 0; i < m; ++i, ++im)
    {
        for (size_t j = 1, jn = 0; j < n; ++j, ++jn)
        {
            if (s[jn] == t[im])
            {
                d[(i * n) + j] = d[((i - 1) * n) + (j - 1)];
            }
            else
            {
                d[(i * n) + j] = std::min(d[(i - 1) * n + j] + 1, /* A deletion. */
                    std::min(d[i * n + (j - 1)] + 1, /* An insertion. */
                        d[(i - 1) * n + (j - 1)] + 1)); /* A substitution. */
            }
        }
    }

#ifdef DEBUG_PRINT
    for (size_t i = 0; i < m; ++i)
    {
        for (size_t j = 0; j < n; ++j)
        {
            cout << d[(i * n) + j] << " ";
        }
        cout << endl;
    }
#endif

    size_t r = d[n * m - 1];

 

    return r;
}