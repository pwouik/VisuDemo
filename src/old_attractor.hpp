//this files contains previous attempt at conceiving an architecture for attractor. The one in mathracttor.hpp is a little to resctirctieve


/*
namespace mtl{ //math utl or whatever I lacked inspiration for naming this namespace
    class matGenerator {
    public:
        virtual glm::mat4 getMatrix() const = 0;
        virtual void ui() = 0;
        virtual ~matGenerator() = default;
    };

    class RawMatrix : public matGenerator {
    public:
        glm::mat4 matrix;
        RawMatrix(const glm::mat4& mat) : matrix(mat) {
            std::cout << "rawm matrix created" << std::endl; //TODO REMOVE
        }

        void ui(){
            ui::valf("raw matrix : ", matrix);
        }
        glm::mat4 getMatrix() const override {
            return matrix;
        }
    };

    class Scale : public matGenerator {
    public:
        glm::vec3 factors;
        Scale(const glm::vec3& factors) : factors(factors) {}

        void ui(){
            ui::valf("sacle,", factors);
        }
        glm::mat4 getMatrix() const override {
            return glm::scale(glm::mat4(1.0f), factors);
        }
    };

    class Rotation : public matGenerator {
    public:
        glm::vec3 axis;
        float angle;
        Rotation(const glm::vec3& axis, float angle) : axis(axis), angle(angle) {}

        void ui(){
            //todo
        }

        glm::mat4 getMatrix() const override {
            return glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis);
        }
    };

    class Shear : public matGenerator {
        glm::vec3 shearFactors;
    public:
        Shear(const glm::vec3& shearFactors) : shearFactors(shearFactors) {}

        glm::mat4 getMatrix() const override {
            glm::mat4 mat(1.0f);
            mat[1].x = shearFactors.x; // Shear in X
            mat[0].y = shearFactors.y; // Shear in Y
            mat[2].x = shearFactors.z; // Shear in Z (optional adjustment)
            return mat;
        }
    };

    class Translation : public matGenerator {
    public:
        glm::vec3 translationVector;
        Translation(const glm::vec3& translationVector) : translationVector(translationVector) {}

        void ui(){
            //todo
        }
        glm::mat4 getMatrix() const override {
            return glm::translate(glm::mat4(1.0f), translationVector);
        }
    };


    class AttractorMatrices {
    public:
        std::vector<matGenerator*> generators;
        AttractorMatrices(const std::vector<matGenerator*>& generators) : generators(generators) {
             std::cout << "attractor matrices created" << std::endl; //TODO REMOVE
        }

        void ui(){
            ImGui::NewLine();
            ImGui::Text("Attractor");
            for (auto& generator : generators) {
                generator->ui();
            }
        }
        glm::mat4 getMatrix() const {
            glm::mat4 result(1.0f);
            for (const auto& generator : generators) {
                result *= generator->getMatrix();
            }
            return result;
        }
    };

    //lerp - per matrix coefficient on 2 matrix genned by attractors A and B
    glm::mat4 matrixLerp(const AttractorMatrices& attractorA, const AttractorMatrices& attractorB, float t) {
        glm::mat4 matA = attractorA.getMatrix();
        glm::mat4 matB = attractorB.getMatrix();
        return (1.0f - t) * matA + t * matB;
    }

    class Attractor{
    public:
        std::vector<AttractorMatrices> attractorsMatrices;

        Attractor(){
            std::cout << "c moi qui fcasse tout" << std::endl;
        }

        void ui(){
            for(auto& am : attractorsMatrices){
                am.ui();
            }
        }
    };

    //init 2 tractors with default value for sierpinski and reverse sierpinski
    //there's like a morbillions useless allocation in this code but this will do for now
    void set_sierpinski(){
        {
            ubo_matrices.clear();
            ubo_matrices.emplace_back(glm::mat4(1.0f));
            ubo_matrices.emplace_back(glm::mat4(1.0f));
            ubo_matrices.emplace_back(glm::mat4(1.0f));

            mtl::RawMatrix* rma1 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.5f,  0.0f, 0.36f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            std::vector<mtl::matGenerator*> vmga1;
            vmga1.push_back(rma1);
            mtl::AttractorMatrices ama1(vmga1);
            mtl::RawMatrix* rma2 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, -0.5f,
                    0.0f, 0.5f,  0.0f, -0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            std::vector<mtl::matGenerator*> vmga2;
            vmga1.push_back(rma2);
            mtl::AttractorMatrices ama2(vmga2);
            mtl::RawMatrix* rma3 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.5f,
                    0.0f, 0.5f,  0.0f, -0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            std::vector<mtl::matGenerator*> vmga3;
            vmga1.push_back(rma3);
            mtl::AttractorMatrices ama3(vmga3);
            mtl::RawMatrix* rmb1 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.5f,  0.0f, -0.36f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            std::vector<mtl::matGenerator*> vmgb1;
            vmga1.push_back(rmb1);
            mtl::AttractorMatrices amb1(vmgb1);
            mtl::RawMatrix* rmb2 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, 0.5f,
                    0.0f, 0.5f,  0.0f, 0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            std::vector<mtl::matGenerator*> vmgb2;
            vmga1.push_back(rmb2);
            mtl::AttractorMatrices amb2(vmgb2);
            mtl::RawMatrix* rmb3 = new mtl::RawMatrix(glm::transpose(glm::mat4(
                    0.5f, 0.0f,  0.0f, -0.5f,
                    0.0f, 0.5f,  0.0f, 0.5f,
                    0.0f, 0.0f,  0.0f, 0.0f,
                    0.0f, 0.0f,  0.0f, 1.0f)));
            std::vector<mtl::matGenerator*> vmgb3;
            vmga1.push_back(rmb3);
            mtl::AttractorMatrices amb3(vmgb3);

            A_tractor.attractorsMatrices.push_back(ama1);
            A_tractor.attractorsMatrices.push_back(ama2);
            A_tractor.attractorsMatrices.push_back(ama3);
            B_tractor.attractorsMatrices.push_back(amb1);
            B_tractor.attractorsMatrices.push_back(amb2);
            B_tractor.attractorsMatrices.push_back(amb3);
            matrixPerAttractor = 3;

            //TODO TODO TODO WARNING TO REMOVE MEMORY LEAK
        }
    }

/**/