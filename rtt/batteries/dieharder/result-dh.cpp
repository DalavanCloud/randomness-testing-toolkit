#include "result-dh.h"

namespace rtt {
namespace batteries {
namespace dieharder {

std::unique_ptr<Result> Result::getInstance(
        const std::vector<ITest *> & tests) {
    std::unique_ptr<Result> r (new Result());

    r->objectInfo = "Dieharder result processor";

    static const std::regex RE_SUBTEST_SPLIT
    {
        "#={77}#([+0-9\\.\\n]*?)#={77}#"
    };

    static const std::regex RE_PVALUE {
        "\\+\\+\\+\\+([01]\\.[0-9]+?)\\+\\+\\+\\+\\n"
    };
    auto endIt = std::sregex_iterator();

    std::vector<SubTestResult> tmpSubTestResults;
    std::vector<PValueSet> tmpPValueSets;
    std::vector<double> tmpPVals;

    /* Single test object processing */
    for(const ITest * test : tests) {

        /* Single variant processing */
        for(const IVariant * variant : test->getVariants()) {
            auto variantOutput =
                    variant->getBatteryOutput().getStdOut();
            auto subTestIt = std::sregex_iterator(
                                 variantOutput.begin(), variantOutput.end(),
                                 RE_SUBTEST_SPLIT);

            /* Single subtest processing! */
            for(; subTestIt != endIt ; ++subTestIt) {
                std::smatch subTestMatch = *subTestIt;
                std::string subTestPVals = subTestMatch[1].str();
                auto pValIt = std::sregex_iterator(
                                  subTestPVals.begin(), subTestPVals.end(),
                                  RE_PVALUE);

                /* Single pvalue processing */
                for( ; pValIt != endIt ; ++pValIt) {
                    std::smatch pvalMatch = *pValIt;
                    tmpPVals.push_back(Utils::strtod(pvalMatch[1].str()));
                }
                double statResult = r->kstest(tmpPVals);
                tmpPValueSets.push_back(
                            PValueSet::getInstance(
                                "Kolmogorov-Smirnov",
                                statResult,
                                std::move(tmpPVals)));
                tmpSubTestResults.push_back(
                            SubTestResult::getInstance(
                                std::move(tmpPValueSets)));
            }
            r->varRes.push_back(VariantResult::getInstance(
                                    std::move(tmpSubTestResults)));
        }
    }

    r->evaluateSetPassed();
    return r;
}

std::vector<VariantResult> Result::getResults() const {
    return varRes;
}

bool Result::getPassed() const {
    return passed;
}

void Result::evaluateSetPassed() {
    std::vector<double> allPValues;
    for(const VariantResult & var : varRes) {
        std::vector<double> tmp = var.getSubTestStatResults();
        allPValues.insert(allPValues.end(), tmp.begin(), tmp.end());
    }

    double exp = 1.0/(double)allPValues.size();
    double alpha = 1.0 - (std::pow(Constants::MATH_ALPHA, exp));

    for(const double & pval : allPValues) {
        if(pval < alpha - Constants::MATH_EPS ||
           pval > 1.0 - alpha + Constants::MATH_EPS) {
            passed = false;
            break;
        }
    }
}

/* Following code is taken from DIEHARDER battery. */
/* Used for calculation of final pvalue of test. */
double Result::kstest(const std::vector<double> & pvalue) {
    int i;
    double y/*,d*/,d1/*,d2*/,dmax;
    double p;
    int count = pvalue.size();

    if (count < 1)
        throw RTTException(objectInfo , Strings::BATT_ERR_KS_NO_PSAMPLES);
    if (count == 1) return pvalue[0];

    dmax = 0.0;
    for(i=1;i<=count;i++) {
        y = (double) i/(count+1.0);
        d1 = pvalue[i-1] - y;
        //d2 = fabs(1.0/(count+1.0) - d1);
        d1 = fabs(d1);
        //d = fmax(d1,d2);

        if(d1 > dmax) dmax = d1;
    }
    p = p_ks_new(count,dmax);
    return p;
}

double Result::p_ks_new(int n, double d) {
    int k,m,i,j,g,eH,eQ;
    double h,s,*H,*Q;
    /*
     * The next fragment is used if ks_test is not 2.  This is faster
     * than going to convergence, but is still really slow compared to
     * switching to the asymptotic form.
     *
     * If you require >7 digit accuracy in the right tail use ks_test = 2
     * but be prepared for occasional long runtimes.
     */
    s=d*d*n;
    if(/*ks_test != 2 && */ ( s>7.24 || ( s>3.76 && n>99 ))) {
        if(n == 10400) printf("Returning the easy way\n");
        return 2.0*exp(-(2.000071+.331/sqrt(n)+1.409/n)*s);
    }

    /*
     * If ks_test = 2, we always execute the following code and work to
     * convergence.
     */
    k=(int)(n*d)+1;
    m=2*k-1;
    h=k-n*d;
    /* printf("p_ks_new:  n = %d  k = %d  m = %d  h = %f\n",n,k,m,h); */
    H=(double*)malloc((m*m)*sizeof(double));
    Q=(double*)malloc((m*m)*sizeof(double));
    for(i=0;i<m;i++){
        for(j=0;j<m;j++){
            if(i-j+1<0){
                H[i*m+j]=0;
            } else {
                H[i*m+j]=1;
            }
        }
    }

    for(i=0;i<m;i++){
        H[i*m]-=pow(h,i+1);
        H[(m-1)*m+i]-=pow(h,(m-i));
    }

    H[(m-1)*m]+=(2*h-1>0?pow(2*h-1,m):0);
    for(i=0;i<m;i++){
        for(j=0;j<m;j++){
            if(i-j+1>0){
                for(g=1;g<=i-j+1;g++){
                    H[i*m+j]/=g;
                }
            }
        }
    }

    eH=0;
    mPower(H,eH,Q,&eQ,m,n);
    /* printf("p_ks_new eQ = %d\n",eQ); */
    s=Q[(k-1)*m+k-1];
    /* printf("s = %16.8e\n",s); */
    for(i=1;i<=n;i++){
        s=s*i/n;
        /* printf("i = %d: s = %16.8e\n",i,s); */
        if(s<1e-140){
            /* printf("Oops, starting to have underflow problems: s = %16.8e\n",s); */
            s*=1e140;
            eQ-=140;
        }
    }

    /* printf("I'll bet this is it: s = %16.8e  eQ = %d\n",s,eQ); */
    s*=pow(10.,eQ);
    s = 1.0 - s;
    free(H);
    free(Q);
    return s;
}

void Result::mMultiply(double *A, double *B, double *C, int m) {
    int i,j,k;
    double s;
    for(i=0; i<m; i++){
        for(j=0; j<m; j++){
            s=0.0;
            for(k=0; k<m; k++){
                s+=A[i*m+k]*B[k*m+j];
                C[i*m+j]=s;
            }
        }
    }
}

void Result::mPower(double *A, int eA, double *V, int *eV, int m, int n) {
    double *B;
    int eB,i,j;

    /*
       * n == 1: first power just returns A.
       */
    if(n == 1){
        for(i=0;i<m*m;i++){
            V[i]=A[i];*eV=eA;
        }
        return;
    }

    /*
       * This is a recursive call.  Either n/2 will equal 1 (and the line
       * above will return and the recursion will terminate) or it won't
       * and we will cumulate the product.
       */
    mPower(A,eA,V,eV,m,n/2);
    /* printf("n = %d  mP eV = %d\n",n/2,*eV); */
    B=(double*)malloc((m*m)*sizeof(double));
    mMultiply(V,V,B,m);
    eB=2*(*eV);
    if(n%2==0){
        for(i=0;i<m*m;i++){
            V[i]=B[i];
        }
        *eV=eB;
        /* printf("n = %d (even) eV = %d\n",n,*eV); */
    } else {
        mMultiply(A,B,V,m);
        *eV=eA+eB;
        /* printf("n = %d (odd) eV = %d\n",n,*eV); */
    }

    /*
       * Rescale as needed to avoid overflow.  Note that we check
       * EVERY element of V to make sure NONE of them exceed the
       * threshold (and if any do, rescale the whole thing).
       */
    for(i=0;i<m*m;i++) {
        if( V[i] > 1.0e140 ) {
            for(j=0;j<m*m;j++) {
                V[j]=V[j]*1.0e-140;
            }
            *eV+=140;
            /* printf("rescale eV = %d\n",*eV); */
        }
    }

    free(B);
}

} // namespace dieharder
} // namespace batteries
} // namespace rtt
