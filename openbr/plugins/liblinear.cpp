#include <QTemporaryFile>
#include <opencv2/core/core.hpp>
#include <opencv2/ml/ml.hpp>

#include "openbr_internal.h"
#include "openbr/core/opencvutils.h"

#include <linear.h>
#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

using namespace cv;

namespace br
{

class LinearSVM : public Transform
{
    Q_OBJECT
    Q_ENUMS(Solver)
    Q_PROPERTY(Solver solver READ get_solver WRITE set_solver RESET reset_solver STORED false)
    Q_PROPERTY(float C READ get_C WRITE set_C RESET reset_C STORED false)
    Q_PROPERTY(QString inputVariable READ get_inputVariable WRITE set_inputVariable RESET reset_inputVariable STORED false)
    Q_PROPERTY(QString outputVariable READ get_outputVariable WRITE set_outputVariable RESET reset_outputVariable STORED false)
    Q_PROPERTY(bool returnDFVal READ get_returnDFVal WRITE set_returnDFVal RESET reset_returnDFVal STORED false)
    Q_PROPERTY(int termCriteria READ get_termCriteria WRITE set_termCriteria RESET reset_termCriteria STORED false)
    Q_PROPERTY(int folds READ get_folds WRITE set_folds RESET reset_folds STORED false)
    Q_PROPERTY(bool balanceFolds READ get_balanceFolds WRITE set_balanceFolds RESET reset_balanceFolds STORED false)

public:
    enum Solver { L2R_LR,
                  L2R_L2LOSS_SVC_DUAL,
                  L2R_L2LOSS_SVC,
                  L2R_L1LOSS_SVC_DUAL,
                  MCSVM_CS,
                  L1R_L2LOSS_SVC,
                  L1R_LR,
                  L2R_LR_DUAL,
                  L2R_L2LOSS_SVR,
                  L2R_L2LOSS_SVR_DUAL,
                  L2R_L1LOSS_SVR_DUAL };

private:
    BR_PROPERTY(Solver, solver, L2R_L2LOSS_SVC_DUAL)
    BR_PROPERTY(float, C, 1)
    BR_PROPERTY(QString, inputVariable, "Label")
    BR_PROPERTY(QString, outputVariable, "")
    BR_PROPERTY(bool, returnDFVal, false)
    BR_PROPERTY(int, termCriteria, 1000)
    BR_PROPERTY(int, folds, 5)
    BR_PROPERTY(bool, balanceFolds, false)

    model *m;

    void train(const TemplateList &data)
    {
        Mat samples = OpenCVUtils::toMat(data.data());
        Mat labels = OpenCVUtils::toMat(File::get<float>(data, inputVariable));

        // Number of features = n
        // Number of instances = l

        problem prob;
        prob.n = samples.cols;
        prob.l = samples.rows;
        prob.bias = -1;
        prob.y = new double[prob.l];

        for (int i=0; i<prob.l; i++)
            prob.y[i] = labels.at<float>(i,0);

        // Allocate enough memory for l feature_nodes pointers
        prob.x = new feature_node*[prob.l];
        feature_node *x_space = new feature_node[(prob.n+1)*prob.l];

        int k = 0;
        for (int i=0; i<prob.l; i++) {
            prob.x[i] = &x_space[k];
            for (int j=0; j<prob.n; j++) {
                x_space[k].index = j+1;
                x_space[k].value = samples.at<float>(i,j);
                k++;
            }
            x_space[k++].index = -1;
        }

        parameter param;
        // TODO: Support grid search
        param.C = C;
        param.eps = FLT_EPSILON;
        param.solver_type = solver;

        // TODO: Support weights
        param.nr_weight = 0;
        param.p = 1;
        param.weight_label = NULL;
        param.weight = NULL;

        m = train_svm(&prob, &param);

        delete[] prob.y;
        delete[] prob.x;
        delete[] x_space;
    }

    void project(const Template &src, Template &dst) const
    {
        dst = src;

        Mat sample = src.m().reshape(1,1);
        feature_node *x_space = new feature_node[sample.cols+1];

        // Assign the address of the ith instance to be the address of the jth feature
        for (int j=0; j<sample.cols; j++) {
            x_space[j].index = j+1;
            x_space[j].value = sample.at<float>(0,j);
        }
        x_space[sample.cols].index = -1;

        // TODO: Call appropriate function based on solver
        double prob_estimates[1];
        float prediction = predict_values(m,x_space,prob_estimates);

        dst.m() = Mat(1, 1, CV_32F);
        dst.m().at<float>(0, 0) = prob_estimates[0];

        delete[] x_space;
    }

    void store(QDataStream &stream) const
    {
        QString filename = QString::number(qrand());
        stream << filename;
        save_model(filename.toStdString().c_str(),m);
    }

    void load(QDataStream &stream)
    {
        QString filename;
        stream >> filename;
        m = load_model(filename.toStdString().c_str());
    }
};

BR_REGISTER(Transform, LinearSVM)

} // namespace br

#include "liblinear.moc"
