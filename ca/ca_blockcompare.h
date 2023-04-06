#ifndef _CA_BLOCK_COMPARE_H_
#define _CA_BLOCK_COMPARE_H_

#include "block.pb.h"

struct CBlockCompare
{
    bool operator()(const CBlock &a, const CBlock &b) const
    {
        if (a.height() > b.height())
        {
            return true;
        }
        else if (a.height() == b.height())
        {
            if (a.time() > b.time())
            {
                return true;
            }
        }
        return false;
    }
};

#endif 
