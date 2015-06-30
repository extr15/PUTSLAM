/** @file RANSAC.c
 *
 * \brief The robust estimation method to find transformation based on two sets of 3f features
 * \author Michal Nowicki
 *
 */
#include "../include/TransformEst/RANSAC.h"
#include "../include/TransformEst/g2oEst.h"
#include "../include/RGBD/RGBD.h"

#include "../include/USAC/USAC_utils.h"

RANSAC::RANSAC(RANSAC::parameters _RANSACParameters, cv::Mat _cameraMatrix) {
//RANSAC::RANSAC(RANSAC::parameters _RANSACParameters, cv::Mat _cameraMatrix) : sensorModel("fileModel.xml") {
	srand(time(0));

	cameraMatrix = _cameraMatrix;

	RANSACParams.verbose = _RANSACParameters.verbose;
	RANSACParams.errorVersion = _RANSACParameters.errorVersion;
	RANSACParams.usedPairs = _RANSACParameters.usedPairs;
	RANSACParams.inlierThresholdEuclidean =
			_RANSACParameters.inlierThresholdEuclidean;
	RANSACParams.inlierThresholdReprojection =
            _RANSACParameters.inlierThresholdReprojection;
    RANSACParams.inlierThresholdMahalanobis =
            _RANSACParameters.inlierThresholdMahalanobis;
	RANSACParams.minimalInlierRatioThreshold =
			_RANSACParameters.minimalInlierRatioThreshold;

	RANSACParams.iterationCount = computeRANSACIteration(0.20);

	if (RANSACParams.verbose > 0) {
		std::cout << "RANSACParams.verbose --> " << RANSACParams.verbose
				<< std::endl;
		std::cout << "RANSACParams.usedPairs --> " << RANSACParams.usedPairs
				<< std::endl;
		std::cout << "RANSACParams.errorVersion --> "
				<< RANSACParams.errorVersion << std::endl;
		std::cout << "RANSACParams.inlierThresholdEuclidean --> "
				<< RANSACParams.inlierThresholdEuclidean << std::endl;
        std::cout << "RANSACParams.inlierThresholdMahalanobis --> "
                << RANSACParams.inlierThresholdMahalanobis << std::endl;
		std::cout << "RANSACParams.inlierThresholdReprojection --> "
				<< RANSACParams.inlierThresholdReprojection << std::endl;
		std::cout << "RANSACParams.minimalInlierRatioThreshold --> "
				<< RANSACParams.minimalInlierRatioThreshold << std::endl;
	}
}

// TODO: MISSING:
// - test of minimal inlierRatio of bestModel
Eigen::Matrix4f RANSAC::estimateTransformation(
		std::vector<Eigen::Vector3f> prevFeatures,
		std::vector<Eigen::Vector3f> features, std::vector<cv::DMatch> matches,
		std::vector<cv::DMatch> & bestInlierMatches) {

	Eigen::Matrix4f bestTransformationModel = Eigen::Matrix4f::Identity();
	float bestInlierRatio = 0.0;

	// Remove matches with features containing invalid depth
	// TODO: It is slow due to the vector rebuilds
	for (std::vector<cv::DMatch>::iterator it = matches.begin();
			it != matches.end();) {
		int prevId = it->queryIdx, id = it->trainIdx;

		if (prevFeatures[prevId].hasNaN() || features[id].hasNaN()
				|| prevFeatures[prevId][2] < 0.1 || prevFeatures[prevId][2] > 6
				|| features[id][2] < 0.1 || features[id][2] > 6)
			it = matches.erase(it);
		else
			++it;
	}

	if (RANSACParams.verbose > 0)
		std::cout << "RANSAC: matches.size() = " << matches.size() << std::endl;

	// TODO: DO IT NICER!
	if (matches.size() < 8) {
		return Eigen::Matrix4f::Identity();
	}

	for (int i = 0; i < RANSACParams.iterationCount; i++) {
		// Randomly select matches
		if (RANSACParams.verbose > 1)
			std::cout << "RANSAC: randomly sampling ids of matches"
					<< std::endl;
        //std::vector<cv::DMatch> randomMatches = getRandomMatches(matches);
		std::vector<cv::DMatch> randomMatches = getNonRandomMatches(i);

		// Compute model based on those matches
		if (RANSACParams.verbose > 1)
			std::cout << "RANSAC: computing model based on matches"
					<< std::endl;
		Eigen::Matrix4f transformationModel;
		bool modelComputation = computeTransformationModel(prevFeatures,
				features, randomMatches, transformationModel);

		// TODO: Check if the model is feasible ?
		bool correctModel = checkModelFeasibility(transformationModel);
		if (correctModel && modelComputation) {
			// Evaluate the model
			if (RANSACParams.verbose > 1)
				std::cout << "RANSAC: evaluating the model" << std::endl;
			std::vector<cv::DMatch> modelConsistentMatches;

			// Choose proper error computation version based on provided parameters
            float inlierRatio = 0;
            if ((RANSACParams.errorVersion == EUCLIDEAN_ERROR) ||
                (RANSACParams.errorVersion == ADAPTIVE_ERROR)){
        		if (RANSACParams.verbose > 1) {
        			std::cout << "Evaluation using Euclidean" << std::endl;
        		}

				inlierRatio = computeInlierRatioEuclidean(prevFeatures,
						features, matches, transformationModel,
						modelConsistentMatches);
			} else if (RANSACParams.errorVersion == REPROJECTION_ERROR) {
				if (RANSACParams.verbose > 1) {
					std::cout << "Evaluation using Reprojection" << std::endl;
				}

				inlierRatio = computeInlierRatioReprojection(prevFeatures,
						features, matches, transformationModel,
						modelConsistentMatches);
			} else if (RANSACParams.errorVersion == EUCLIDEAN_AND_REPROJECTION_ERROR) {
				if (RANSACParams.verbose > 1) {
					std::cout << "Evaluation using EuclideanAndReprojection" << std::endl;
				}

				inlierRatio = computeInlierRatioEuclideanAndReprojection(
						prevFeatures, features, matches, transformationModel,
						modelConsistentMatches);
            } else if (RANSACParams.errorVersion == MAHALANOBIS_ERROR) {
        		if (RANSACParams.verbose > 1) {
        			std::cout << "Evaluation using Mahalanobis" << std::endl;
        		}

                inlierRatio = computeInlierRatioMahalanobis(
                        prevFeatures, features, matches, transformationModel,
                        modelConsistentMatches);
            }else
				std::cout << "RANSAC: incorrect error version" << std::endl;

			// Save better model
			if (RANSACParams.verbose > 1)
				std::cout << "RANSAC: saving best model" << std::endl;
			saveBetterModel(inlierRatio, transformationModel,
					modelConsistentMatches, bestInlierRatio,
					bestTransformationModel, bestInlierMatches);

			// Print achieved result
			if (RANSACParams.verbose > 1)
				std::cout << "RANSAC: best model inlier ratio : "
						<< bestInlierRatio * 100.0 << "%" << std::endl;
		}

	}

	// Reestimate from inliers
	computeTransformationModel(prevFeatures, features, bestInlierMatches,
			bestTransformationModel, UMEYAMA);
	std::vector<cv::DMatch> newBestInlierMatches;
	computeInlierRatioEuclidean(prevFeatures, features, bestInlierMatches,
			bestTransformationModel, newBestInlierMatches);
	newBestInlierMatches.swap(bestInlierMatches);

	// Test the number of inliers
	if (bestInlierRatio < RANSACParams.minimalInlierRatioThreshold) {
		bestTransformationModel = Eigen::Matrix4f::Identity();
	}

	// Test for minimal inlierRatio of bestModel
	if (RANSACParams.verbose > 0) {
		std::cout << "RANSAC best model : inlierRatio = "
				<< bestInlierRatio * 100.0 << "%" << std::endl;
		std::cout << "RANSAC best model : " << std::endl
				<< bestTransformationModel << std::endl;
	}
	return bestTransformationModel;
}

// TODO: MISSING:
// - checking against deadlock
// - check if chosen points are not too close to each other
//
std::vector<cv::DMatch> RANSAC::getRandomMatches(
		const std::vector<cv::DMatch> matches) {
	const int matchesSize = matches.size();

	std::vector<cv::DMatch> chosenMatches;
	std::vector<bool> validIndex(matchesSize, true);

	// Loop until we found enough matches
	while (chosenMatches.size() < RANSACParams.usedPairs) {

		// Randomly sample one match
		int sampledMatchIndex = rand() % matchesSize;

		// Check if the match was not already chosen or is not marked as wrong
		if (validIndex[sampledMatchIndex] == true) {

			// Add sampled match
			chosenMatches.push_back(matches[sampledMatchIndex]);

			// Prevent choosing it again
			validIndex[sampledMatchIndex] = false;
		}
	}

	return chosenMatches;
}

// TODO:
// - how to handle Grisetti version?
bool RANSAC::computeTransformationModel(
		const std::vector<Eigen::Vector3f> prevFeatures,
		const std::vector<Eigen::Vector3f> features,
		const std::vector<cv::DMatch> matches,
		Eigen::Matrix4f &transformationModel, TransfEstimationType usedType) {

	std::cout << "Random matches used: " << std::endl;
	for(auto match : matches)
	{
		std::cout << match.queryIdx << ", " << match.trainIdx << ", " << match.distance << std::endl;
	}

	Eigen::MatrixXf prevFeaturesMatrix(matches.size(), 3), featuresMatrix(
			matches.size(), 3);

	// Create matrices
	for (int j = 0; j < matches.size(); j++) {
		cv::DMatch p = matches[j];
		prevFeaturesMatrix.block<1, 3>(j, 0) = prevFeatures[p.queryIdx];
		featuresMatrix.block<1, 3>(j, 0) = features[p.trainIdx];
	}

	// Compute transformation
	if (usedType == UMEYAMA) {
		std::cout << "Using umeyama for transformation model" << std::endl;
		transformationModel = Eigen::umeyama(featuresMatrix.transpose(),
				prevFeaturesMatrix.transpose(), false);
	} else if (usedType == G2O) {
		std::cout << "Using g2oEst for transformation model" << std::endl;
		putslam::TransformEst* g2oEst = createG2OEstimator();
		Mat34 transformation = g2oEst->computeTransformation(
				featuresMatrix.cast<double>().transpose(),
				prevFeaturesMatrix.cast<double>().transpose());
		transformationModel = transformation.cast<float>().matrix();
	} else {
		std::cout << "RANSAC: unrecognized transformation estimation"
				<< std::endl;
	}

	std::cout << "transformation: " << std::endl << transformationModel << std::endl;

	// Check if it failed
	if (std::isnan(transformationModel(0, 0))) {
		transformationModel = Eigen::Matrix4f::Identity();
		return false;
	}
	return true;
}

// TODO: - model feasibility
bool RANSAC::checkModelFeasibility(Eigen::Matrix4f transformationModel) {
	return true;
}

float RANSAC::computeInlierRatioEuclidean(
		const std::vector<Eigen::Vector3f> prevFeatures,
		const std::vector<Eigen::Vector3f> features,
		const std::vector<cv::DMatch> matches,
		const Eigen::Matrix4f transformationModel,
		std::vector<cv::DMatch> &modelConsistentMatches) {
	// Break into rotation (R) and translation (t)
	Eigen::Matrix3f R = transformationModel.block<3, 3>(0, 0);
	Eigen::Vector3f t = transformationModel.block<3, 1>(0, 3);

	int inlierCount = 0;
	// For all matches
	for (std::vector<cv::DMatch>::const_iterator it = matches.begin();
			it != matches.end(); ++it) {
		// Estimate location of feature from position one after transformation
        Eigen::Vector3f estimatedOldPosition = R * features[it->trainIdx] + t;

		// Compute residual error and compare it to inlier threshold
        float_type threshold = RANSACParams.inlierThresholdEuclidean;
        if (RANSACParams.errorVersion == ADAPTIVE_ERROR)
            threshold *= prevFeatures[it->queryIdx].z();
		if ((estimatedOldPosition - prevFeatures[it->queryIdx]).norm()
                < threshold) {
			inlierCount++;
			modelConsistentMatches.push_back(*it);
		}
	}

	std::cout << "Inlier count: " << inlierCount << std::endl;

	// Percent of correct matches
	return float(inlierCount) / matches.size();
}

float RANSAC::computeInlierRatioMahalanobis(
        const std::vector<Eigen::Vector3f> prevFeatures,
        const std::vector<Eigen::Vector3f> features,
        const std::vector<cv::DMatch> matches,
        const Eigen::Matrix4f transformationModel,
        std::vector<cv::DMatch> &modelConsistentMatches) {
    // Break into rotation (R) and translation (t)
    Eigen::Matrix3f R = transformationModel.block<3, 3>(0, 0);
    Eigen::Vector3f t = transformationModel.block<3, 1>(0, 3);

    int inlierCount = 0;
    // For all matches
    for (std::vector<cv::DMatch>::const_iterator it = matches.begin();
            it != matches.end(); ++it) {
        // Estimate location of feature from position one after transformation
        Eigen::Vector3f estimatedOldPosition = R * features[it->trainIdx] + t;

        // Compute residual error and compare it to inlier threshold
        Mat33 cov;
        //sensorModel.computeCov(prevFeatures[it->queryIdx],cov);
        if (cov.determinant()!=0){
            double distMah = (estimatedOldPosition - prevFeatures[it->queryIdx]).transpose()*cov.cast<float>()*(estimatedOldPosition - prevFeatures[it->queryIdx]);
            /*std::cout << "vec: " << estimatedOldPosition.x() << " -> " << prevFeatures[it->queryIdx].x() <<"\n";
            std::cout << "vec: " << estimatedOldPosition.y() << " -> " << prevFeatures[it->queryIdx].y() <<"\n";
            std::cout << "vec: " << estimatedOldPosition.z() << " -> " << prevFeatures[it->queryIdx].z() <<"\n";
            std::cout << "distMah: " << distMah << " distEucl: " << (estimatedOldPosition - prevFeatures[it->queryIdx]).norm() << "\n";
            std::cout << "distMahThr: " << RANSACParams.inlierThresholdMahalanobis << "\n";
            getchar();*/
            if (distMah < RANSACParams.inlierThresholdMahalanobis) {
                inlierCount++;
                modelConsistentMatches.push_back(*it);
            }
        }
    }

    // Percent of correct matches
    return float(inlierCount) / matches.size();
}

float RANSAC::computeInlierRatioReprojection(
		const std::vector<Eigen::Vector3f> prevFeatures,
		const std::vector<Eigen::Vector3f> features,
		const std::vector<cv::DMatch> matches,
		const Eigen::Matrix4f transformationModel,
		std::vector<cv::DMatch> &modelConsistentMatches) {

	// Break into rotation (R) and translation (t)
	Eigen::Matrix3f R = transformationModel.block<3, 3>(0, 0);
	Eigen::Vector3f t = transformationModel.block<3, 1>(0, 3);

	// Break into rotation (R) and translation (t)
	Eigen::Matrix3f Rinv = transformationModel.inverse().block<3, 3>(0, 0);
	Eigen::Vector3f tinv = transformationModel.inverse().block<3, 1>(0, 3);

	int inlierCount = 0;
	// For all matches
	for (std::vector<cv::DMatch>::const_iterator it = matches.begin();
			it != matches.end(); ++it) {
		// Estimate location of feature from position one after transformation
		Eigen::Vector3f estimatedOldPosition = R * features[it->trainIdx] + t;
		Eigen::Vector3f estimatedNewPosition = Rinv * prevFeatures[it->queryIdx]
				+ tinv;

		// Now project both features
		cv::Point2f predictedNew = RGBD::point3Dto2D(estimatedNewPosition,
				cameraMatrix);
		cv::Point2f realNew = RGBD::point3Dto2D(features[it->trainIdx],
				cameraMatrix);

		cv::Point2f predictedOld = RGBD::point3Dto2D(estimatedOldPosition,
				cameraMatrix);
		cv::Point2f realOld = RGBD::point3Dto2D(prevFeatures[it->queryIdx],
				cameraMatrix);

		// Compute residual error and compare it to inlier threshold
		double error2D[2] { cv::norm(predictedNew - realNew), cv::norm(
				predictedOld - realOld) };

		// Compute residual error and compare it to inlier threshold
		if (error2D[0] < RANSACParams.inlierThresholdReprojection
				&& error2D[1] < RANSACParams.inlierThresholdReprojection) {

			inlierCount++;
			modelConsistentMatches.push_back(*it);
		}
	}

	// Percent of correct matches
	return float(inlierCount) / matches.size();
}

float RANSAC::computeInlierRatioEuclideanAndReprojection(
		const std::vector<Eigen::Vector3f> prevFeatures,
		const std::vector<Eigen::Vector3f> features,
		const std::vector<cv::DMatch> matches,
		const Eigen::Matrix4f transformationModel,
		std::vector<cv::DMatch> &modelConsistentMatches) {
	// Break into rotation (R) and translation (t)
	Eigen::Matrix3f R = transformationModel.block<3, 3>(0, 0);
	Eigen::Vector3f t = transformationModel.block<3, 1>(0, 3);

	// Break into rotation (R) and translation (t)
	Eigen::Matrix3f Rinv = transformationModel.inverse().block<3, 3>(0, 0);
	Eigen::Vector3f tinv = transformationModel.inverse().block<3, 1>(0, 3);

	int inlierCount = 0;
	// For all matches
	for (std::vector<cv::DMatch>::const_iterator it = matches.begin();
			it != matches.end(); ++it) {
		// Estimate location of feature from position one after transformation
		Eigen::Vector3f estimatedOldPosition = R * features[it->trainIdx] + t;
		Eigen::Vector3f estimatedNewPosition = Rinv * prevFeatures[it->queryIdx]
				+ tinv;

		// Compute error3D
		double error3D =
				(estimatedOldPosition - prevFeatures[it->queryIdx]).norm();

		// Now project both features
		cv::Point2f predictedNew = RGBD::point3Dto2D(estimatedNewPosition,
				cameraMatrix);
		cv::Point2f realNew = RGBD::point3Dto2D(features[it->trainIdx],
				cameraMatrix);

		cv::Point2f predictedOld = RGBD::point3Dto2D(estimatedOldPosition,
				cameraMatrix);
		cv::Point2f realOld = RGBD::point3Dto2D(prevFeatures[it->queryIdx],
				cameraMatrix);

		// Compute residual error and compare it to inlier threshold
		double error2D[2] { cv::norm(predictedNew - realNew), cv::norm(
				predictedOld - realOld) };

		//std::cout<<"ERROR : " << error3D << " " << error2D[0] << " " << error2D[1] << std::endl;

		// Compute residual error and compare it to inlier threshold
		if (error3D < RANSACParams.inlierThresholdEuclidean
				&& error2D[0] < RANSACParams.inlierThresholdReprojection
				&& error2D[1] < RANSACParams.inlierThresholdReprojection) {
//			std::cout<<"3D position: " << estimatedOldPosition.transpose() << "\t" << prevFeatures[it->queryIdx].transpose() << std::endl;
//			std::cout<<"3D position: " << estimatedNewPosition.transpose() << "\t" << features[it->trainIdx].transpose() << std::endl;
//			std::cout<<"ERROR : " << error3D << " " << (estimatedNewPosition - features[it->trainIdx]).norm() <<" "<< error2D[0] << " " << error2D[1] << std::endl;

			inlierCount++;
			modelConsistentMatches.push_back(*it);
		}
	}

	// Percent of correct matches
	return float(inlierCount) / matches.size();
}

inline void RANSAC::saveBetterModel(const float inlierRatio,
		const Eigen::Matrix4f transformationModel,
		std::vector<cv::DMatch> modelConsistentMatches, float &bestInlierRatio,
		Eigen::Matrix4f & bestTransformationModel,
		std::vector<cv::DMatch> &bestInlierMatches) {
	if (inlierRatio > bestInlierRatio) {
		// Save better model
		std::cout << "Actually really saving the model" << std::endl;
		bestTransformationModel = transformationModel;
		bestInlierRatio = inlierRatio;
		bestInlierMatches.swap(modelConsistentMatches);

		// Update iteration count
		RANSACParams.iterationCount = std::min(computeRANSACIteration(RANSACParams.minimalInlierRatioThreshold), computeRANSACIteration(bestInlierRatio));
	}
}

inline int RANSAC::computeRANSACIteration(double inlierRatio,
		double successProbability, int numberOfPairs) {
	return (log(1 - successProbability)
			/ log(1 - pow(inlierRatio, numberOfPairs)));
}

